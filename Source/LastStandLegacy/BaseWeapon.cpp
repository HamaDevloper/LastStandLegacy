#include "BaseWeapon.h"
#include "Hama.h"
#include "HamaComponent.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Engine.h"
#include "Curves/CurveVector.h"
#include "LastStandLegacyGameState.h"
#include "RecoilComponent.h"
#include "DamageableInterface.h"
#include "Animation/AimOffsetBlendSpace1D.h"
#include "DrawDebugHelpers.h"

ABaseWeapon::ABaseWeapon()
{
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;

    bReplicates = true;

    SetNetUpdateFrequency(40.f);
    SetMinNetUpdateFrequency(10.f);

    WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
    RootComponent = WeaponMesh;

    NextAllowedFireTime = 0.f;
}

void ABaseWeapon::BeginPlay()
{
    Super::BeginPlay();
    InitializeWeaponData();
    UpdateCachedReferences();
}

void ABaseWeapon::OnRep_Owner()
{
    Super::OnRep_Owner();
    UpdateCachedReferences();
}

void ABaseWeapon::UpdateCachedReferences()
{
    OwnerCharacter = Cast<AHama>(GetOwner());
    if (!OwnerCharacter) return;

    HamaComponent = OwnerCharacter->FindComponentByClass<UHamaComponent>();
    OwnerController = Cast<APlayerController>(OwnerCharacter->GetController());
    if (OwnerController)
    {
        CacheRecoil();
    }
}

void ABaseWeapon::InitializeWeaponData()
{
    if (!WeaponDataTable || WeaponRowName.IsNone()) return;

    FWeaponData* Row = WeaponDataTable->FindRow<FWeaponData>(WeaponRowName, TEXT("Weapon Context"));
    if (!Row) return;

    CurrentWeaponData = *Row;

    MaxAmmoInClip = CurrentWeaponData.MaxAmmoInClip;
    CurrentAmmo = MaxAmmoInClip;
    ReserveAmmo = CurrentWeaponData.MaxReserveAmmo;
    Damage = CurrentWeaponData.Damage;

    if (HasAuthority())
    {
        MARK_PROPERTY_DIRTY_FROM_NAME(ABaseWeapon, MaxAmmoInClip, this);
        MARK_PROPERTY_DIRTY_FROM_NAME(ABaseWeapon, CurrentAmmo, this);
        MARK_PROPERTY_DIRTY_FROM_NAME(ABaseWeapon, ReserveAmmo, this);
        MARK_PROPERTY_DIRTY_FROM_NAME(ABaseWeapon, Damage, this);
    }

    if (!IsRunningDedicatedServer() && WeaponMesh && !CurrentWeaponData.WeaponMeshAsset.IsNull())
    {
        USkeletalMesh* Mesh = CurrentWeaponData.WeaponMeshAsset.Get();

        if (!Mesh)
        {
#if !UE_BUILD_SHIPPING
            UE_LOG(LogTemp, Warning, TEXT("[BaseWeapon] Mesh '%s' was NOT preloaded! Synchronous load fallback triggered."), *CurrentWeaponData.WeaponMeshAsset.ToString());
#endif
            Mesh = CurrentWeaponData.WeaponMeshAsset.LoadSynchronous();
        }

        if (Mesh)
        {
            WeaponMesh->SetSkeletalMeshAsset(Mesh);
        }
    }
}

void ABaseWeapon::OnRep_WeaponRowName()
{
    InitializeWeaponData();
}

void ABaseWeapon::EquipWeapon(AHama* NewOwnerCharacter)
{
    if (!NewOwnerCharacter) return;

    SetOwner(NewOwnerCharacter);
    UpdateCachedReferences();
}

void ABaseWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Params;
    Params.bIsPushBased = true;

    Params.Condition = COND_None;
    DOREPLIFETIME_WITH_PARAMS_FAST(ABaseWeapon, WeaponRowName, Params);

    Params.Condition = COND_SkipOwner;
    DOREPLIFETIME_WITH_PARAMS_FAST(ABaseWeapon, bIsReloading, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(ABaseWeapon, BurstCounter, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(ABaseWeapon, CurrentAmmo, Params);

    Params.Condition = COND_OwnerOnly;
    DOREPLIFETIME_WITH_PARAMS_FAST(ABaseWeapon, ReserveAmmo, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(ABaseWeapon, Damage, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(ABaseWeapon, MaxAmmoInClip, Params);
}

float ABaseWeapon::GetEffectiveFireRate() const
{
    float ActualFireRate = CurrentWeaponData.FireRate;
    if (OwnerCharacter && OwnerCharacter->HasDoubleTap())
    {
        ActualFireRate = FMath::Max(ActualFireRate / 1.33f, 0.0f);
    }
    return ActualFireRate;
}

void ABaseWeapon::StartFire()
{
    if (!OwnerCharacter) return;
    if (GetWorldTimerManager().IsTimerActive(FireTimerHandle)) return;

    if (CurrentAmmo <= 0 && ReserveAmmo <= 0 && !IsInfiniteAmmoActive())
    {
        OwnerCharacter->AutoSwapToAvailableWeapon();
        return;
    }

    if (bIsReloading)
    {
        if (CurrentAmmo > 0) CancelReload();
        else return;
    }

    float CurrentTime = GetWorld()->GetTimeSeconds();

    if (CurrentWeaponData.FireMode == EWeaponFireMode::Burst)
    {
        CurrentBurstShotsLeft = CurrentWeaponData.BurstShotCount;
    }

    if (CurrentTime >= NextAllowedFireTime)
    {
        HandleFireLocal();

        if (CurrentWeaponData.FireMode == EWeaponFireMode::Automatic || CurrentWeaponData.FireMode == EWeaponFireMode::Burst)
        {
            GetWorldTimerManager().SetTimer(
                FireTimerHandle, this, &ABaseWeapon::HandleFireLocal,
                GetEffectiveFireRate(), true
            );
        }
    }
}

void ABaseWeapon::StopFire()
{
    if (CurrentWeaponData.FireMode == EWeaponFireMode::Burst)
    {
        CurrentBurstShotsLeft = 0;
    }
    ShotsFiredInBurst = 0;

    GetWorldTimerManager().ClearTimer(FireTimerHandle);
}

float ABaseWeapon::CalculateBulletSpread()
{
    if (HamaComponent && HamaComponent->IsAiming()) return 0.f;

    float CurrentSpread = CurrentWeaponData.BulletSpread;

    if (OwnerCharacter && OwnerCharacter->GetCharacterMovement())
    {
        UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement();
        if (MoveComp->IsCrouching()) CurrentSpread *= CurrentWeaponData.CrouchSpreadMultiplier;
        else if (MoveComp->IsFalling()) CurrentSpread *= CurrentWeaponData.AirSpreadMultiplier;
    }

    if (OwnerCharacter && OwnerCharacter->HasDeadshot()) CurrentSpread *= 0.65f;

    return CurrentSpread;
}

float ABaseWeapon::CalculateDamageBySurface(const FHitResult& Hit)
{
    float ActualDamage = Damage;
    FString SurfaceStr = TEXT("BODY / DEFAULT");
    FColor PrintColor = FColor::White;

    if (Hit.PhysMaterial.IsValid())
    {
        UPhysicalMaterial* PhysMat = Hit.PhysMaterial.Get();
        EPhysicalSurface SurfaceType = UPhysicalMaterial::DetermineSurfaceType(PhysMat);

        if (SurfaceType == EPhysicalSurface::SurfaceType1) // Headshot
        {
            ActualDamage *= CurrentWeaponData.HeadshotMultiplier;
            SurfaceStr = TEXT("HEADSHOT");
            PrintColor = FColor::Green;
        }
        else if (SurfaceType == EPhysicalSurface::SurfaceType2) // Leg
        {
            ActualDamage *= CurrentWeaponData.LegDamageMultiplier;
            SurfaceStr = TEXT("LEGSHOT");
            PrintColor = FColor::Yellow;
        }

        if (GEngine)
        {
            FString Msg = FString::Printf(TEXT("[%s] Hit Bone: %s | PhysMat: %s | SurfaceType: %d | Type: %s | Damage: %.1f"),
                HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"),
                *Hit.BoneName.ToString(),
                *PhysMat->GetName(),
                (int32)SurfaceType,
                *SurfaceStr,
                ActualDamage);

            GEngine->AddOnScreenDebugMessage(1, 5.0f, PrintColor, Msg);
        }
    }

    return ActualDamage;
}

bool ABaseWeapon::IsInfiniteAmmoActive() const
{
    if (GSCache) return GSCache->bIsGlobalBulletStormActive;

    if (UWorld* World = GetWorld())
    {
        if (ALastStandLegacyGameState* GS = World->GetGameState<ALastStandLegacyGameState>())
        {
            return GS->bIsGlobalBulletStormActive;
        }
    }
    return false;
}

void ABaseWeapon::HandleFireLocal()
{
    if (!OwnerCharacter || !OwnerCharacter->IsLocallyControlled()) return;

    if (!OwnerController)
    {
        OwnerController = Cast<APlayerController>(OwnerCharacter->GetController());
        if (!OwnerController) return;
    }

    if (CurrentAmmo <= 0 && !IsInfiniteAmmoActive())
    {
        Reload();
        StopFire();
        return;
    }

    if (CurrentWeaponData.FireMode == EWeaponFireMode::Burst)
    {
        if (CurrentBurstShotsLeft <= 0) { StopFire(); return; }
        CurrentBurstShotsLeft--;
    }

    if (!IsInfiniteAmmoActive())
    {
        CurrentAmmo = FMath::Max(0, CurrentAmmo - 1);
        OnAmmoChanged.ExecuteIfBound(CurrentAmmo, ReserveAmmo);
    }

    BurstCounter = (BurstCounter > 255) ? 1 : (BurstCounter + 1);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(2001, 6.f, FColor::Cyan,
            FString::Printf(TEXT("[CLIENT] CurrentAmmo=%d | ReserveAmmo=%d"), CurrentAmmo, ReserveAmmo));
    }

    if (ShootForceFeedback)
    {
        FForceFeedbackParameters FeedbackParams;
        FeedbackParams.bLooping = false;
        FeedbackParams.Tag = TEXT("WeaponFire");
        OwnerController->ClientPlayForceFeedback(ShootForceFeedback, FeedbackParams);
    }

    const int32 RequestedShots = (OwnerCharacter->HasDoubleTap()) ? 2 : 1;

    float CurrentTime = GetWorld()->GetTimeSeconds();
    NextAllowedFireTime = CurrentTime + GetEffectiveFireRate();

    if (HamaComponent && HamaComponent->IsSprinting()) HamaComponent->StopSprinting();

    ApplyRecoilAndCameraShake();
    PlayWeaponEffects();

    FVector CameraLoc;
    FRotator CameraRot;
    OwnerController->GetPlayerViewPoint(CameraLoc, CameraRot);

    const FVector AimDir = CameraRot.Vector();

    const bool bIsAiming = (HamaComponent && HamaComponent->IsAiming());
    const float SpreadDegrees = CalculateBulletSpread();
    const float SpreadRadians = FMath::DegreesToRadians(SpreadDegrees);
    const float MinDoubleTapDivergenceRad = bIsAiming ? 0.f : FMath::DegreesToRadians(0.75f);

    TArray<FVector_NetQuantizeNormal> ShotDirections;

    for (int i = 0; i < RequestedShots; i++)
    {
        float CurrentSpreadRad = SpreadRadians;
        if (i == 1 && CurrentSpreadRad < MinDoubleTapDivergenceRad)
        {
            CurrentSpreadRad = MinDoubleTapDivergenceRad;
        }

        FVector ShootDir = AimDir;
        if (CurrentSpreadRad > 0.f)
        {
            ShootDir = FMath::VRandCone(AimDir, CurrentSpreadRad);
        }

        ShotDirections.Add(ShootDir);
    }

    Server_ProcessShot(CameraLoc, ShotDirections);

    if ((CurrentAmmo <= 0 && !IsInfiniteAmmoActive()) || CurrentWeaponData.FireMode == EWeaponFireMode::Single)
    {
        StopFire();
        if (CurrentAmmo <= 0 && !IsInfiniteAmmoActive()) Reload();
    }
}


void ABaseWeapon::Server_ProcessShot_Implementation(FVector_NetQuantize MuzzleLocation, const TArray<FVector_NetQuantizeNormal>& ShotDirections)
{
    if (!OwnerCharacter) return;

    float CurrentTime = GetWorld()->GetTimeSeconds();

    const float BaseToleranceSq = 100000.f;
    const float VelocityToleranceSq = OwnerCharacter->GetVelocity().SizeSquared() * 0.0225f;
    float MaxAllowedDistSq = BaseToleranceSq + VelocityToleranceSq;
    float DistSquared = FVector::DistSquared(OwnerCharacter->GetActorLocation(), MuzzleLocation);
    int32 MaxAllowedShots = OwnerCharacter->HasDoubleTap() ? 2 : 1;

    bool bFireRateViolation = CurrentTime < (ServerNextAllowedFireTime - 0.15f);
    bool bMuzzleSpoofing = DistSquared > MaxAllowedDistSq;
    bool bTooManyShots = ShotDirections.Num() > MaxAllowedShots;


    if (bFireRateViolation || bMuzzleSpoofing || bTooManyShots)
    {
        FString RejectReason = TEXT("");
        if (bFireRateViolation) RejectReason += TEXT("FireRate ");
        if (bMuzzleSpoofing) RejectReason += TEXT("MuzzleSpoofing ");
        if (bTooManyShots) RejectReason += TEXT("TooManyShots ");

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
                FString::Printf(TEXT("[SERVER] Shot Rejected! Reason: %s| Syncing Ammo..."), *RejectReason));
        }

        Client_ForceSyncAmmo(CurrentAmmo);
        return;
    }

    if (!OwnerCharacter->IsLocallyControlled())
    {
        if (!IsInfiniteAmmoActive())
        {
            if (CurrentAmmo <= 0) return;

            CurrentAmmo = FMath::Max(0, CurrentAmmo - 1);
            MARK_PROPERTY_DIRTY_FROM_NAME(ABaseWeapon, CurrentAmmo, this);

            BurstCounter = (BurstCounter > 255) ? 1 : (BurstCounter + 1);
        }
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(2002, 6.f, FColor::Orange,
            FString::Printf(TEXT("[SERVER-RECEIVED] CurrentAmmo=%d | ReserveAmmo=%d"), CurrentAmmo, ReserveAmmo));
    }

    ServerNextAllowedFireTime = CurrentTime + GetEffectiveFireRate();

    for (const FVector_NetQuantizeNormal& Dir : ShotDirections)
    {
        ProcessShotLogic(MuzzleLocation, Dir);
    }
}

void ABaseWeapon::ProcessShotLogic(const FVector& TraceStart, const FVector& ShootDir)
{
    if (!HasAuthority() || !OwnerCharacter) return;

    const FVector TraceEnd = TraceStart + (ShootDir * CurrentWeaponData.MaxRange);

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(OwnerCharacter);
    Params.bReturnPhysicalMaterial = true;
    Params.bTraceComplex = false;

    AController* InstigatorController = OwnerCharacter->GetController();
    TSubclassOf<UDamageType> DamageTypeClass = UDamageType::StaticClass();

    TArray<FHitResult> HitResults;
    bool bHit = GetWorld()->LineTraceMultiByChannel(HitResults, TraceStart, TraceEnd, ECollisionChannel::ECC_Bullet, Params);

    TArray<AActor*> AlreadyHitActors;
    int32 PenCount = 0;
    const int32 MaxPen = FMath::Max(1, CurrentWeaponData.MaxZombiePenetration);

    for (const FHitResult& Hit : HitResults)
    {
        AActor* HitActor = Hit.GetActor();
        if (!HitActor) continue;

        IDamageableInterface* Damageable = Cast<IDamageableInterface>(HitActor);

        if (!Damageable)
        {
            break;
        }

        if (!Damageable->CanReceiveWeaponDamage())
        {
            continue;
        }

        if (AlreadyHitActors.Contains(HitActor)) continue;
        AlreadyHitActors.Add(HitActor);

        float FinalDamage = CalculateDamageBySurface(Hit);

        UGameplayStatics::ApplyPointDamage(
            HitActor,
            FinalDamage,
            ShootDir,
            Hit,
            InstigatorController,
            this,
            DamageTypeClass
        );

        PenCount++;
        if (PenCount >= MaxPen) break;
    }
}

void ABaseWeapon::Client_ForceSyncAmmo_Implementation(int32 ServerAmmo)
{
    CurrentAmmo = ServerAmmo;
    OnAmmoChanged.ExecuteIfBound(CurrentAmmo, ReserveAmmo);
}

void ABaseWeapon::PlayLocalHitEffects(const FHitResult& LocalHit)
{
    // UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BloodEffect, LocalHit.ImpactPoint);
}

void ABaseWeapon::OnRep_BurstCounter()
{
    PlayWeaponEffects();
}

void ABaseWeapon::PlayWeaponEffects()
{
    // VFX & SFX
}

// -----------------------------------------------------

void ABaseWeapon::ApplyRecoilAndCameraShake()
{
    if (!OwnerController || !OwnerCharacter || !OwnerCharacter->IsLocallyControlled()) return;

    if (CurrentWeaponData.FireCameraShake)
    {
        OwnerController->ClientStartCameraShake(CurrentWeaponData.FireCameraShake);
    }

    if (!RecoilComponent)
    {
        CacheRecoil();
    }

    if (RecoilComponent)
    {
        float Multiplier = (HamaComponent && HamaComponent->IsAiming()) ? CurrentWeaponData.AimRecoilMultiplier : 1.0f;

        RecoilComponent->AddRecoil(
            CurrentWeaponData.RecoilPitchCurve,
            ShotsFiredInBurst,
            Multiplier,
            CurrentWeaponData.RecoilRandomness,
            CurrentWeaponData.RecoilRecoverySpeed
        );

        ShotsFiredInBurst++;
    }
}

URecoilComponent* ABaseWeapon::CacheRecoil()
{
    if (RecoilComponent) return RecoilComponent;

    if (OwnerController)
    {
        RecoilComponent = OwnerController->FindComponentByClass<URecoilComponent>();
    }

    return RecoilComponent;
}

ALastStandLegacyGameState* ABaseWeapon::GetGameStateCache() const
{
    if (!GSCache)
    {
        if (UWorld* World = GetWorld())
        {
            GSCache = World->GetGameState<ALastStandLegacyGameState>();
        }
    }
    return GSCache;
}

bool ABaseWeapon::HasAmmo() const
{
    return  CurrentAmmo > 0 || ReserveAmmo > 0;
}

bool ABaseWeapon::NeedsAmmo() const
{
    return  (ReserveAmmo < CurrentWeaponData.MaxReserveAmmo);
}

UAnimSequence* ABaseWeapon::GetAimSequence() const
{
    if (UAnimSequence* Anim = CurrentWeaponData.AimSequence.Get())
    {
        return Anim;
    }

    if (!CurrentWeaponData.AimSequence.IsNull())
    {
        FString DebugMsg = FString::Printf(TEXT("[%s] AimSequence missing from Preload! Synchronous load triggered."), *GetName());
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, DebugMsg);
        }

        return CurrentWeaponData.AimSequence.LoadSynchronous();
    }
    return nullptr;
}

UAnimSequence* ABaseWeapon::GetWeaponIdle() const
{
    if (UAnimSequence* Anim = CurrentWeaponData.WeaponIdle.Get())
    {
        return Anim;
    }

    if (!CurrentWeaponData.WeaponIdle.IsNull())
    {
        FString DebugMsg = FString::Printf(TEXT("[%s] WeaponIdle missing from Preload! Synchronous load triggered."), *GetName());

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, DebugMsg);
        }

        return CurrentWeaponData.WeaponIdle.LoadSynchronous();
    }
    return nullptr;
}

UAnimSequence* ABaseWeapon::GetWeaponSprint() const
{
    if (UAnimSequence* Anim = CurrentWeaponData.WeaponSprint.Get())
    {
        return Anim;
    }

    if (!CurrentWeaponData.WeaponSprint.IsNull())
    {
        FString DebugMsg = FString::Printf(TEXT("[%s] WeaponSprint missing from Preload! Synchronous load triggered."), *GetName());

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, DebugMsg);
        }

        return CurrentWeaponData.WeaponSprint.LoadSynchronous();
    }
    return nullptr;
}

UAimOffsetBlendSpace1D* ABaseWeapon::GetAimOffsetAsset() const
{
    if (UAimOffsetBlendSpace1D* AO = CurrentWeaponData.AimOffsetAsset.Get())
    {
        return AO;
    }

    if (!CurrentWeaponData.AimOffsetAsset.IsNull())
    {
        FString DebugMsg = FString::Printf(TEXT("[%s] AimOffsetAsset missing from Preload! Synchronous load triggered."), *GetName());

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, DebugMsg);
        }

        return CurrentWeaponData.AimOffsetAsset.LoadSynchronous();
    }
    return nullptr;
}

UAnimMontage* ABaseWeapon::GetReloadMontage() const
{
    if (UAnimMontage* Montage = CurrentWeaponData.ReloadMontage.Get())
    {
        return Montage;
    }

    if (!CurrentWeaponData.ReloadMontage.IsNull())
    {
        FString DebugMsg = FString::Printf(TEXT("[%s] ReloadMontage missing from Preload! Synchronous load triggered."), *GetName());

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, DebugMsg);
        }

        return CurrentWeaponData.ReloadMontage.LoadSynchronous();
    }
    return nullptr;
}

// ------------------- RELOAD REFACTOR -------------------

void ABaseWeapon::RefillAmmo()
{
    if (!HasAuthority()) return;

    bool bWasEmpty = (CurrentAmmo <= 0 && ReserveAmmo <= 0);

    ReserveAmmo = CurrentWeaponData.MaxReserveAmmo;
    MARK_PROPERTY_DIRTY_FROM_NAME(ABaseWeapon, ReserveAmmo, this);

    if (OwnerCharacter->IsLocallyControlled())
    {
        OnAmmoChanged.ExecuteIfBound(CurrentAmmo, ReserveAmmo);
    }

    if (bWasEmpty)
    {
        if (OwnerCharacter->IsLocallyControlled())
        {
            if (OwnerCharacter->IsSprinting()) OwnerCharacter->StopSprint();
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("LocalReloadStarded"));
            Reload();
        }
    }

    Client_ForceReload(ReserveAmmo, bWasEmpty);
}

void ABaseWeapon::Client_ForceReload_Implementation(int32 NewReserveAmmo, bool bCanReload)
{
    ReserveAmmo = NewReserveAmmo;

    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("InsideClient"));
    if (OwnerCharacter->IsLocallyControlled())
    {
        OnAmmoChanged.ExecuteIfBound(CurrentAmmo, ReserveAmmo);

        if (bCanReload)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("ClientReloadStarted"));
            if (OwnerCharacter->IsSprinting()) OwnerCharacter->StopSprint();
            Reload();
        }
    }
}

float ABaseWeapon::GetCalculatedReloadTime() const
{
    UAnimMontage* ReloadMontage = GetReloadMontage();
    float BaseTime = ReloadMontage ? ReloadMontage->GetPlayLength() : CurrentWeaponData.DefaultReloadTime;

    ALastStandLegacyGameState* GS = GetGameStateCache();
    bool bHasFastReload = (GS && GS->IsTeamAdrenalineActive()) || (OwnerCharacter && OwnerCharacter->HasFastHands());

    return bHasFastReload ? (BaseTime / 2.0f) : BaseTime;
}

void ABaseWeapon::Reload()
{
    if (ReserveAmmo <= 0)
    {
        if (CurrentAmmo <= 0 && OwnerCharacter && OwnerCharacter->IsLocallyControlled())
        {
            OwnerCharacter->AutoSwapToAvailableWeapon();
        }
        return;
    }

    if (CurrentAmmo >= MaxAmmoInClip || bIsReloading || !OwnerCharacter || !OwnerCharacter->IsLocallyControlled()) return;

    GetWorldTimerManager().ClearTimer(ReloadTimerHandle);
    bIsReloading = true;

    const float FinalReloadTime = GetCalculatedReloadTime();
    UAnimMontage* ReloadMontage = GetReloadMontage();

    if (ReloadMontage)
    {
        const float PlayRate = (FinalReloadTime < ReloadMontage->GetPlayLength()) ? 2.0f : 1.0f;
        OwnerCharacter->PlayAnimMontage(ReloadMontage, PlayRate);
    }

    if (!HasAuthority())
    {
        ServerReload();
        GetWorldTimerManager().SetTimer(ReloadTimerHandle, this, &ABaseWeapon::Local_ReloadComplete, FinalReloadTime, false);
    }
    else
    {
        ServerReload_Implementation();
    }
}

void ABaseWeapon::Local_ReloadComplete()
{
    if (!OwnerCharacter || !OwnerCharacter->IsLocallyControlled()) return;

    int32 AmmoNeeded = MaxAmmoInClip - CurrentAmmo;
    int32 AmmoToMove = FMath::Min(AmmoNeeded, ReserveAmmo);

    CurrentAmmo += AmmoToMove;
    ReserveAmmo -= AmmoToMove;
    bIsReloading = false;

    OnAmmoChanged.ExecuteIfBound(CurrentAmmo, ReserveAmmo);

    if (OwnerCharacter->bIsFireButtonHold) StartFire();
}

void ABaseWeapon::ServerReload_Implementation()
{
    bIsReloading = true;
    MARK_PROPERTY_DIRTY_FROM_NAME(ABaseWeapon, bIsReloading, this);

    if (OwnerCharacter && !OwnerCharacter->IsLocallyControlled()) OnRep_Reload();

    float FinalReloadTime = GetCalculatedReloadTime();

    GetWorldTimerManager().SetTimer(ReloadTimerHandle, this, &ABaseWeapon::Server_ReloadComplete, FinalReloadTime, false);
}

void ABaseWeapon::Server_ReloadComplete()
{
    int32 AmmoNeeded = MaxAmmoInClip - CurrentAmmo;
    int32 AmmoToMove = FMath::Min(AmmoNeeded, ReserveAmmo);

    CurrentAmmo += AmmoToMove;
    ReserveAmmo -= AmmoToMove;
    bIsReloading = false;

    MARK_PROPERTY_DIRTY_FROM_NAME(ABaseWeapon, CurrentAmmo, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(ABaseWeapon, ReserveAmmo, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(ABaseWeapon, bIsReloading, this);

    if (OwnerCharacter)
    {
        if (OwnerCharacter->IsLocallyControlled())
        {
            OnAmmoChanged.ExecuteIfBound(CurrentAmmo, ReserveAmmo);
            if (OwnerCharacter->bIsFireButtonHold) StartFire();
        }
        else
        {
            OnRep_Reload();
        }
    }
}

void ABaseWeapon::CancelReload()
{
    GetWorldTimerManager().ClearTimer(ReloadTimerHandle);
    bIsReloading = false;

    if (OwnerCharacter)
    {
        USkeletalMeshComponent* MeshComp = OwnerCharacter->GetMesh();
        UAnimMontage* ReloadMontage = GetReloadMontage();

        if (MeshComp && MeshComp->GetAnimInstance() && ReloadMontage)
        {
            MeshComp->GetAnimInstance()->Montage_Stop(0.2f, ReloadMontage);
        }

        if (OwnerCharacter->IsLocallyControlled() && !HasAuthority())
        {
            Server_CancelReload();
        }
        else if (HasAuthority() && !OwnerCharacter->IsLocallyControlled())
        {
            Client_CancelReload();
            OnRep_Reload();
        }
    }
}

void ABaseWeapon::Client_CancelReload_Implementation()
{
    GetWorldTimerManager().ClearTimer(ReloadTimerHandle);
    bIsReloading = false;

    if (OwnerCharacter)
    {
        USkeletalMeshComponent* MeshComp = OwnerCharacter->GetMesh();
        UAnimMontage* ReloadMontage = GetReloadMontage();

        if (MeshComp && MeshComp->GetAnimInstance() && ReloadMontage)
        {
            MeshComp->GetAnimInstance()->Montage_Stop(0.2f, ReloadMontage);
        }
    }
}

void ABaseWeapon::Server_CancelReload_Implementation()
{
    bIsReloading = false;
    MARK_PROPERTY_DIRTY_FROM_NAME(ABaseWeapon, bIsReloading, this);

    GetWorldTimerManager().ClearTimer(ReloadTimerHandle);

    if (OwnerCharacter && !OwnerCharacter->IsLocallyControlled())
    {
        OnRep_Reload();

        USkeletalMeshComponent* MeshComp = OwnerCharacter->GetMesh();
        UAnimMontage* ReloadMontage = GetReloadMontage();

        if (MeshComp && MeshComp->GetAnimInstance() && ReloadMontage)
        {
            MeshComp->GetAnimInstance()->Montage_Stop(0.2f, ReloadMontage);
        }
    }
}

void ABaseWeapon::OnRep_Reload()
{
    if (!OwnerCharacter || OwnerCharacter->IsLocallyControlled()) return;

    USkeletalMeshComponent* MeshComp = OwnerCharacter->GetMesh();
    if (!MeshComp) return;

    UAnimInstance* AnimInstance = MeshComp->GetAnimInstance();
    if (!AnimInstance) return;

    UAnimMontage* ReloadMontage = GetReloadMontage();
    if (!ReloadMontage) return;

    if (bIsReloading)
    {
        float PlayRate = (GetCalculatedReloadTime() < ReloadMontage->GetPlayLength()) ? 2.0f : 1.0f;
        AnimInstance->Montage_Play(ReloadMontage, PlayRate);
    }
    else
    {
        AnimInstance->Montage_Stop(0.2f, ReloadMontage);
    }
}