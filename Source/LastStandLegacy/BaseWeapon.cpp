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
#include "Curves/CurveVector.h"
#include "LastStandLegacyGameState.h"
#include "DrawDebugHelpers.h"

ABaseWeapon::ABaseWeapon()
{
    PrimaryActorTick.bCanEverTick = true;
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

void ABaseWeapon::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!OwnerController) return;

    FRotator PreviousRecoil = CurrentRecoilOffset;

    CurrentRecoilOffset = FMath::RInterpTo(CurrentRecoilOffset, TargetRecoilOffset, DeltaTime, 15.0f);

    float DeltaPitch = CurrentRecoilOffset.Pitch - PreviousRecoil.Pitch;
    float DeltaYaw = CurrentRecoilOffset.Yaw - PreviousRecoil.Yaw;

    OwnerController->AddPitchInput(-DeltaPitch);
    OwnerController->AddYawInput(DeltaYaw);

    if (CurrentRecoilOffset.Equals(TargetRecoilOffset, 0.01f))
    {
        SetActorTickEnabled(false);
    }
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

    if (WeaponMesh && !CurrentWeaponData.WeaponMeshAsset.IsNull())
    {
        TWeakObjectPtr<ABaseWeapon> WeakThis(this);
        UAssetManager::GetStreamableManager().RequestAsyncLoad(
            CurrentWeaponData.WeaponMeshAsset.ToSoftObjectPath(),
            [WeakThis]()
            {
                if (!WeakThis.IsValid()) return;
                if (USkeletalMesh* Mesh = WeakThis->CurrentWeaponData.WeaponMeshAsset.Get())
                {
                    WeakThis->WeaponMesh->SetSkeletalMesh(Mesh);
                }
            }
        );
    }
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

    Params.Condition = COND_SkipOwner;
    DOREPLIFETIME_WITH_PARAMS_FAST(ABaseWeapon, CurrentAmmo, Params);

    Params.Condition = COND_SkipOwner;
    DOREPLIFETIME_WITH_PARAMS_FAST(ABaseWeapon, bIsReloading, Params);

    Params.Condition = COND_SkipOwner;
    DOREPLIFETIME_WITH_PARAMS_FAST(ABaseWeapon, BurstCounter, Params);

    Params.Condition = COND_OwnerOnly;
    DOREPLIFETIME_WITH_PARAMS_FAST(ABaseWeapon, ReserveAmmo, Params);

    Params.Condition = COND_OwnerOnly;
    DOREPLIFETIME_WITH_PARAMS_FAST(ABaseWeapon, Damage, Params);

    Params.Condition = COND_OwnerOnly;
    DOREPLIFETIME_WITH_PARAMS_FAST(ABaseWeapon, MaxAmmoInClip, Params);
   
}

void ABaseWeapon::ServerUpgradeWeapon_PackAPunch_Implementation()
{
    if (!HasAuthority()) return;

    Damage *= 2.f;
    MaxAmmoInClip = FMath::RoundToInt(MaxAmmoInClip * 2.f);
    CurrentWeaponData.MaxReserveAmmo *= 2;
    ReserveAmmo = CurrentWeaponData.MaxReserveAmmo;

    MARK_PROPERTY_DIRTY_FROM_NAME(ABaseWeapon, Damage, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(ABaseWeapon, MaxAmmoInClip, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(ABaseWeapon, ReserveAmmo, this);

    if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
    {
        OnAmmoChanged.ExecuteIfBound(CurrentAmmo, ReserveAmmo);
    }
    else
    {
        Client_ApplyPackAPunchFX(ReserveAmmo);
    }
}

void ABaseWeapon::Client_ApplyPackAPunchFX_Implementation(int32 NewReserveAmmo)
{
    ReserveAmmo = NewReserveAmmo;

    if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
    {
        OnAmmoChanged.ExecuteIfBound(CurrentAmmo, ReserveAmmo);
    }
}

void ABaseWeapon::RefillAmmo()
{
    if (!HasAuthority()) return;

    bool bWasEmpty = (CurrentAmmo <= 0 && ReserveAmmo <= 0);

    ReserveAmmo = CurrentWeaponData.MaxReserveAmmo;

    MARK_PROPERTY_DIRTY_FROM_NAME(ABaseWeapon, ReserveAmmo, this);

    if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
    {
        OnAmmoChanged.ExecuteIfBound(CurrentAmmo, ReserveAmmo);
    }

    if (bWasEmpty)
    {
        if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
        {
            Reload();
        }
        else
        {
            Client_ForceReload(ReserveAmmo);
        }
    }
}

void ABaseWeapon::StartFire()
{
    if (!OwnerCharacter) return;
    if (GetWorldTimerManager().IsTimerActive(FireTimerHandle)) return;

    if (CurrentAmmo <= 0 && ReserveAmmo <= 0 && !IsInfiniteAmmoActive()) return;

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

        if (!HasAuthority())
        {
            Server_StartFire();
        }

        if (CurrentWeaponData.FireMode == EWeaponFireMode::Automatic || CurrentWeaponData.FireMode == EWeaponFireMode::Burst)
        {
            float ActualFireRate = CurrentWeaponData.FireRate;
            if (OwnerCharacter->GetDoubleTap()) ActualFireRate = FMath::Max(ActualFireRate / 1.33f, 0.1f);
            GetWorldTimerManager().SetTimer(
                FireTimerHandle, this, &ABaseWeapon::HandleFireLocal,
                ActualFireRate, true
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

    GetWorldTimerManager().ClearTimer(FireTimerHandle);
    ResetRecoil();

    if (!HasAuthority())
    {
        Server_StopFire();
    }
    else
    {
        Server_StopFire_Implementation();
    }
}

float ABaseWeapon::CalculateBulletSpread()
{
    if (HamaComponent && HamaComponent->IsAiming())
    {
        return 0.f;
    }

    float CurrentSpread = CurrentWeaponData.BulletSpread;

    if (OwnerCharacter && OwnerCharacter->GetCharacterMovement())
    {
        UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement();

        if (MoveComp->IsCrouching())
        {
            CurrentSpread *= CurrentWeaponData.CrouchSpreadMultiplier;
        }
        else if (MoveComp->IsFalling())
        {
            CurrentSpread *= CurrentWeaponData.AirSpreadMultiplier;
        }
    }

    if (OwnerCharacter && OwnerCharacter->HasDeadshot())
    {
        CurrentSpread *= 0.65f;
    }

    return CurrentSpread;
}

float ABaseWeapon::CalculateDamageBySurface(const FHitResult& Hit)
{
    float ActualDamage = Damage;

    if (Hit.PhysMaterial.IsValid())
    {
        EPhysicalSurface SurfaceType = UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());

        if (SurfaceType == EPhysicalSurface::SurfaceType1)
            ActualDamage *= CurrentWeaponData.HeadshotMultiplier;
        else if (SurfaceType == EPhysicalSurface::SurfaceType2)
            ActualDamage *= CurrentWeaponData.LegDamageMultiplier;
    }

    if (IsInfiniteAmmoActive())
    {
        ActualDamage *= 2.0f;
    }

    return ActualDamage;
}

bool ABaseWeapon::IsInfiniteAmmoActive() const
{
    if (GSCache)
    {
        return GSCache->bIsGlobalBulletStormActive;
    }

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

    if (CurrentAmmo <= 0 && !IsInfiniteAmmoActive())
    {
        Reload();
        StopFire();
        return;
    }

    if (CurrentWeaponData.FireMode == EWeaponFireMode::Burst)
    {
        if (CurrentBurstShotsLeft <= 0)
        {
            StopFire();
            return;
        }
        CurrentBurstShotsLeft--;
    }

    if (!OwnerController)
    {
        OwnerController = Cast<APlayerController>(OwnerCharacter->GetController());
        if (!OwnerController) return;
    }

    if (!IsInfiniteAmmoActive())
    {
        CurrentAmmo = FMath::Max(0, CurrentAmmo - 1);
        OnAmmoChanged.ExecuteIfBound(CurrentAmmo, ReserveAmmo);
    }

    float CurrentTime = GetWorld()->GetTimeSeconds();
    float ActualFireRate = CurrentWeaponData.FireRate;
    if (OwnerCharacter->GetDoubleTap()) ActualFireRate = FMath::Max(ActualFireRate / 1.33f, 0.1f);
    NextAllowedFireTime = CurrentTime + ActualFireRate;

    if (HamaComponent && HamaComponent->IsSprinting()) HamaComponent->StopSprinting();

    if (ShootForceFeedback)
    {
        FForceFeedbackParameters FeedbackParams;
        FeedbackParams.bLooping = false;
        FeedbackParams.Tag = FName("WeaponFire");
        OwnerController->ClientPlayForceFeedback(ShootForceFeedback, FeedbackParams);
    }

    ApplyRecoilAndCameraShake();
    PlayWeaponEffects();

    FVector CameraLoc;
    FRotator CameraRot;
    OwnerController->GetPlayerViewPoint(CameraLoc, CameraRot);

    FVector BaseDir = CameraRot.Vector();
    float Spread = CalculateBulletSpread();
    float SpreadInRadians = FMath::DegreesToRadians(Spread);

    BurstCounter = (BurstCounter >= 255) ? 1 : BurstCounter + 1;

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(OwnerCharacter);
    Params.bReturnPhysicalMaterial = true;

    int32 ShotsToFire = (OwnerCharacter->GetDoubleTap()) ? 2 : 1;

    for (int32 ShotIndex = 0; ShotIndex < ShotsToFire; ++ShotIndex)
    {
        FVector LaunchDirection = BaseDir;

        if (SpreadInRadians > 0.0f || ShotIndex > 0)
        {
            FRandomStream WeaponRandomStream;
            WeaponRandomStream.Initialize(BurstCounter + (ShotIndex * 1234));
            float CurrentSpreadRadians = SpreadInRadians;
            if (ShotIndex > 0) CurrentSpreadRadians = FMath::DegreesToRadians(Spread + 0.3f);
            LaunchDirection = WeaponRandomStream.VRandCone(BaseDir, CurrentSpreadRadians);
        }

        FVector EndLocation = CameraLoc + (LaunchDirection.GetSafeNormal() * CurrentWeaponData.MaxRange);
        FVector ShotDirection = LaunchDirection.GetSafeNormal();

        if (CurrentWeaponData.MaxZombiePenetration <= 1)
        {
            FHitResult LocalHit;
            if (GetWorld()->LineTraceSingleByChannel(LocalHit, CameraLoc, EndLocation, ECC_Bullet, Params))
            {
                PlayLocalHitEffects(LocalHit);
                if (LocalHit.GetActor())
                {
                    Server_ApplyDamage(LocalHit.GetActor(), ShotDirection, LocalHit);
                }
            }
        }
        else
        {
            TArray<FHitResult> Hits;
            if (GetWorld()->LineTraceMultiByChannel(Hits, CameraLoc, EndLocation, ECC_Bullet, Params))
            {
                int32 PenetratedCount = 0;
                TSet<AActor*> HitActors;

                for (const FHitResult& SingleHit : Hits)
                {
                    if (!SingleHit.GetActor() || HitActors.Contains(SingleHit.GetActor())) continue;

                    PlayLocalHitEffects(SingleHit);
                    Server_ApplyDamage(SingleHit.GetActor(), ShotDirection, SingleHit);

                    HitActors.Add(SingleHit.GetActor());
                    PenetratedCount++;

                    if (PenetratedCount >= CurrentWeaponData.MaxZombiePenetration || SingleHit.bBlockingHit) break;
                }
            }
        }
    }

    if (HasAuthority())
    {
        Server_FireRoutine();
    }

    if ((CurrentAmmo <= 0 && !IsInfiniteAmmoActive()) || CurrentWeaponData.FireMode == EWeaponFireMode::Single)
    {
        StopFire();
        if (CurrentAmmo <= 0 && !IsInfiniteAmmoActive())
        {
            Reload();
        }
    }
}

void ABaseWeapon::PlayLocalHitEffects(const FHitResult& LocalHit)
{
    // UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BloodEffect, LocalHit.ImpactPoint);
}

// ------------------- سێرڤەر ڕووتین -------------------

void ABaseWeapon::Server_StartFire_Implementation()
{
    if (GetWorldTimerManager().IsTimerActive(ServerFireTimerHandle)) return;

    float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime < NextAllowedFireTime - 0.05f) return;

    if (CurrentWeaponData.FireMode == EWeaponFireMode::Burst)
    {
        ServerBurstShotsLeft = CurrentWeaponData.BurstShotCount;
    }

    Server_FireRoutine();

    if (CurrentWeaponData.FireMode == EWeaponFireMode::Automatic || CurrentWeaponData.FireMode == EWeaponFireMode::Burst)
    {
        float ActualFireRate = CurrentWeaponData.FireRate;
        if (OwnerCharacter && OwnerCharacter->GetDoubleTap())
        {
            ActualFireRate = FMath::Max(ActualFireRate / 1.33f, 0.1f);
        }

        GetWorldTimerManager().SetTimer(
            ServerFireTimerHandle, this, &ABaseWeapon::Server_FireRoutine,
            ActualFireRate, true
        );
    }
}

void ABaseWeapon::Server_StopFire_Implementation()
{
    GetWorldTimerManager().ClearTimer(ServerFireTimerHandle);
}

void ABaseWeapon::Server_FireRoutine()
{
    if ((CurrentAmmo <= 0 && !IsInfiniteAmmoActive()) || bIsReloading || !OwnerCharacter)
    {
        Server_StopFire_Implementation();
        return;
    }

    if (CurrentWeaponData.FireMode == EWeaponFireMode::Burst)
    {
        if (ServerBurstShotsLeft <= 0)
        {
            Server_StopFire_Implementation();
            return;
        }
        ServerBurstShotsLeft--;
    }

    float CurrentTime = GetWorld()->GetTimeSeconds();
    float ActualFireRate = CurrentWeaponData.FireRate;

    if (OwnerCharacter && OwnerCharacter->GetDoubleTap())
    {
        ActualFireRate = FMath::Max(ActualFireRate / 1.33f, 0.1f);
    }

    NextAllowedFireTime = CurrentTime + ActualFireRate;

    if (HasAuthority() && !OwnerCharacter->IsLocallyControlled())
    {
        if (!IsInfiniteAmmoActive())
        {
            CurrentAmmo = FMath::Max(0, CurrentAmmo - 1);

            MARK_PROPERTY_DIRTY_FROM_NAME(ABaseWeapon, CurrentAmmo, this);
        }
    }

    BurstCounter = (BurstCounter >= 255) ? 1 : BurstCounter + 1;

    MARK_PROPERTY_DIRTY_FROM_NAME(ABaseWeapon, BurstCounter, this);
}

void ABaseWeapon::Server_ApplyDamage_Implementation(AActor* HitActor, FVector ShotDirection, FHitResult HitInfo)
{
    if (!HitActor || !OwnerCharacter || !OwnerCharacter->GetController()) return;

    float DistanceToTarget = FVector::Dist(OwnerCharacter->GetActorLocation(), HitActor->GetActorLocation());
    float MaxAllowedDistance = CurrentWeaponData.MaxRange + 500.0f;

    if (DistanceToTarget > MaxAllowedDistance)
    {
        UE_LOG(LogTemp, Warning, TEXT("CHEATING DETECTED: Player tried to shoot target out of weapon range!"));
        return;
    }

    float FinalDamage = CalculateDamageBySurface(HitInfo);

    AController* DamageInstigator = OwnerCharacter->GetController();

    UGameplayStatics::ApplyPointDamage(
        HitActor, FinalDamage, ShotDirection,
        HitInfo, DamageInstigator, this, UDamageType::StaticClass()
    );
}

// -----------------------------------------------------

void ABaseWeapon::ApplyRecoilAndCameraShake()
{
    if (!OwnerController || !OwnerCharacter) return;

    if (CurrentWeaponData.FireCameraShake)
    {
        OwnerController->ClientStartCameraShake(CurrentWeaponData.FireCameraShake);
    }

    if (CurrentWeaponData.RecoilPitchCurve)
    {
        float CurvePitchValue = CurrentWeaponData.RecoilPitchCurve->GetFloatValue(ShotsFiredInBurst);
        float RandomYaw = FMath::RandRange(-CurrentWeaponData.RecoilRandomness, CurrentWeaponData.RecoilRandomness);
        float Multiplier = (HamaComponent && HamaComponent->IsAiming()) ? CurrentWeaponData.AimRecoilMultiplier : 1.0f;

        float TargetPitch = CurvePitchValue * Multiplier;
        float TargetYaw = RandomYaw * Multiplier;

        TargetRecoilOffset = FRotator(TargetPitch, TargetYaw, 0.f);

        ShotsFiredInBurst++;
    }
}

void ABaseWeapon::ResetRecoil()
{
    ShotsFiredInBurst = 0;
    TargetRecoilOffset = FRotator::ZeroRotator;
    CurrentRecoilOffset = FRotator::ZeroRotator;
}

ALastStandLegacyGameState* ABaseWeapon::GetGameStateCache()
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

bool ABaseWeapon::NeedsAmmo() const
{
    return (ReserveAmmo < CurrentWeaponData.MaxReserveAmmo);
}

void ABaseWeapon::OnRep_BurstCounter()
{
    PlayWeaponEffects();
}

void ABaseWeapon::PlayWeaponEffects()
{
    // VFX و دەنگ
}

void ABaseWeapon::Client_ForceReload_Implementation(int32 NewReserveAmmo)
{
    ReserveAmmo = NewReserveAmmo;

    if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
    {
        OnAmmoChanged.ExecuteIfBound(CurrentAmmo, ReserveAmmo);
    }

    Reload();
}

void ABaseWeapon::Reload()
{
    if (ReserveAmmo <= 0 || CurrentAmmo >= MaxAmmoInClip || bIsReloading || !OwnerCharacter || !OwnerCharacter->IsLocallyControlled()) return;

    bIsReloading = true;
    ReloadStartReserveAmmo = ReserveAmmo;
    float ReloadTimeToUse = CurrentWeaponData.DefaultReloadTime;

    if (CurrentWeaponData.ReloadMontage)
    {
        float MontagePlayRate = 1.0f;
        ReloadTimeToUse = CurrentWeaponData.ReloadMontage->GetPlayLength();
        ALastStandLegacyGameState* GS = GetGameStateCache();
        if ((GS && GS->IsTeamAdrenalineActive()) || (OwnerCharacter && OwnerCharacter->HasFastHands()))
        {
            ReloadTimeToUse /= 2;
            MontagePlayRate = 2;
        }
        OwnerCharacter->PlayAnimMontage(CurrentWeaponData.ReloadMontage, MontagePlayRate);
    }

    bool bIsEmpty = (CurrentAmmo <= 0);

    if (!HasAuthority())
    {
        GetWorldTimerManager().SetTimer(ReloadTimerHandle, this, &ABaseWeapon::Local_ReloadComplete, ReloadTimeToUse, false);
        ServerReload(ReloadTimeToUse, bIsEmpty);
    }
    else
    {
        ServerReload_Implementation(ReloadTimeToUse, bIsEmpty);
    }
}

void ABaseWeapon::Local_ReloadComplete()
{
    if (!OwnerCharacter || !OwnerCharacter->IsLocallyControlled()) return;

    int32 AmmoNeeded = MaxAmmoInClip - CurrentAmmo;
    int32 AmmoToMove = FMath::Min(AmmoNeeded, ReloadStartReserveAmmo);
    CurrentAmmo += AmmoToMove;
    bIsReloading = false;

    OnAmmoChanged.ExecuteIfBound(CurrentAmmo, ReserveAmmo);

    if (OwnerCharacter->bIsFireButtonHold) StartFire();
}

void ABaseWeapon::ServerReload_Implementation(float InReloadTime, bool bClipEmpty)
{
    if (bClipEmpty) CurrentAmmo = 0;
    GetWorldTimerManager().ClearTimer(ServerFireTimerHandle);

    bIsReloading = true;

    MARK_PROPERTY_DIRTY_FROM_NAME(ABaseWeapon, bIsReloading, this);

    if (OwnerCharacter && !OwnerCharacter->IsLocallyControlled()) OnRep_Reload();

    float FinalReloadTime = CurrentWeaponData.ReloadMontage ? CurrentWeaponData.ReloadMontage->GetPlayLength() : InReloadTime;
    if ((GetGameStateCache() && GetGameStateCache()->IsTeamAdrenalineActive()) || (OwnerCharacter && OwnerCharacter->HasFastHands()))
        FinalReloadTime /= 2.0f;

    GetWorldTimerManager().SetTimer(ReloadTimerHandle, this, &ABaseWeapon::Server_ReloadComplete, FMath::Max(FinalReloadTime - 0.1f, 0.1f), false);
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
        if (MeshComp && MeshComp->GetAnimInstance() && CurrentWeaponData.ReloadMontage)
        {
            MeshComp->GetAnimInstance()->Montage_Stop(0.2f, CurrentWeaponData.ReloadMontage);
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
        if (MeshComp && MeshComp->GetAnimInstance() && CurrentWeaponData.ReloadMontage)
        {
            MeshComp->GetAnimInstance()->Montage_Stop(0.2f, CurrentWeaponData.ReloadMontage);
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
        if (MeshComp && MeshComp->GetAnimInstance() && CurrentWeaponData.ReloadMontage)
        {
            MeshComp->GetAnimInstance()->Montage_Stop(0.2f, CurrentWeaponData.ReloadMontage);
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

    if (bIsReloading)
    {
        if (CurrentWeaponData.ReloadMontage)
        {
            float MontagePlayRate = 1.0f;

            ALastStandLegacyGameState* GS = GetGameStateCache();
            if ((GS && GS->IsTeamAdrenalineActive()) || (OwnerCharacter && OwnerCharacter->HasFastHands()))
            {
                MontagePlayRate = 2.0f;
            }
            AnimInstance->Montage_Play(CurrentWeaponData.ReloadMontage, MontagePlayRate);
        }
    }
    else
    {
        if (CurrentWeaponData.ReloadMontage)
        {
            AnimInstance->Montage_Stop(0.2f, CurrentWeaponData.ReloadMontage);
        }
    }
}

void ABaseWeapon::OnRep_CurrentAmmo()
{
    if (!bIsReloading)
    {
        OnAmmoChanged.ExecuteIfBound(CurrentAmmo, ReserveAmmo);
    }
}

void ABaseWeapon::OnRep_ReserveAmmo()
{
    if (!bIsReloading)
    {
        OnAmmoChanged.ExecuteIfBound(CurrentAmmo, ReserveAmmo);
    }
}