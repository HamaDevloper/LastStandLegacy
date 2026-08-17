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
    CachedUpgradedClass = Row->UpgradedWeaponClass;

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
    if (OwnerCharacter && OwnerCharacter->GetDoubleTap())
    {
        ActualFireRate = FMath::Max(ActualFireRate / 1.33f, 0.04f);
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

    GetWorldTimerManager().ClearTimer(FireTimerHandle);
    ResetRecoil();
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
    FString HitLocationName = TEXT("Body");

    if (Hit.PhysMaterial.IsValid())
    {
        EPhysicalSurface SurfaceType = UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());

        if (SurfaceType == EPhysicalSurface::SurfaceType1)
        {
            ActualDamage *= CurrentWeaponData.HeadshotMultiplier;
            HitLocationName = TEXT("HEAD");
        }
        else if (SurfaceType == EPhysicalSurface::SurfaceType2)
        {
            ActualDamage *= CurrentWeaponData.LegDamageMultiplier;
            HitLocationName = TEXT("LEG");
        }
    }

    // -------------------------------------------------------------
    // DEBUG OUTPUT (Print String)
    // -------------------------------------------------------------
    FString DebugMsg = FString::Printf(TEXT("[%s] Shot Hit: %s | Final Damage: %.1f"),
        HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"),
        *HitLocationName,
        ActualDamage);

    FColor MsgColor = (HitLocationName == TEXT("HEAD")) ? FColor::Green :
        (HitLocationName == TEXT("LEG") ? FColor::Yellow : FColor::White);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(1, 3.0f, MsgColor, DebugMsg);
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

    if (ShootForceFeedback)
    {
        FForceFeedbackParameters FeedbackParams;
        FeedbackParams.bLooping = false;
        FeedbackParams.Tag = TEXT("WeaponFire");
        OwnerController->ClientPlayForceFeedback(ShootForceFeedback, FeedbackParams);
    }

    const int32 RequestedShots = (OwnerCharacter->GetDoubleTap()) ? 2 : 1;

    float CurrentTime = GetWorld()->GetTimeSeconds();
    NextAllowedFireTime = CurrentTime + GetEffectiveFireRate();

    if (HamaComponent && HamaComponent->IsSprinting()) HamaComponent->StopSprinting();

    ApplyRecoilAndCameraShake();
    PlayWeaponEffects();

    FVector CameraLoc;
    FRotator CameraRot;
    OwnerController->GetPlayerViewPoint(CameraLoc, CameraRot);

    FVector CameraTraceEnd = CameraLoc + (CameraRot.Vector() * CurrentWeaponData.MaxRange);

    FCollisionQueryParams CameraParams(SCENE_QUERY_STAT(WeaponCameraTrace), false, OwnerCharacter);
    CameraParams.AddIgnoredActor(this); // زۆر گرنگە: بۆ ئەوەی ترەیسەکە بەر خودی چەکەکە نەکەوێت و لاری نەکاتەوە!
    CameraParams.AddIgnoredActor(OwnerCharacter);

    FHitResult ScreenHit;
    bool bHitScreen = GetWorld()->LineTraceSingleByChannel(ScreenHit, CameraLoc, CameraTraceEnd, ECC_Visibility, CameraParams);

    // خاڵی ئامانج
    FVector TargetLocation = bHitScreen ? ScreenHit.ImpactPoint : CameraTraceEnd;

    FName MuzzleSocket = CurrentWeaponData.MuzzleLocationName;
    FVector MuzzleLoc = (WeaponMesh && WeaponMesh->DoesSocketExist(MuzzleSocket)) ? WeaponMesh->GetSocketLocation(MuzzleSocket) : CameraLoc;

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

        // دروستکردنی ئاڕاستەی بنەڕەتی ڕێکە ڕێک
        FVector BaseDirection = (TargetLocation - MuzzleLoc).GetSafeNormal();

        // ئەگەر ئامانجگرتن تەواو بوو (سفر Spread)، ڕێک BaseDirection بەکاربهێنە بێ ئەوەی بیخەیتە ناو VRandCone
        FVector ShootDir = BaseDirection;
        if (CurrentSpreadRad > 0.f)
        {
            ShootDir = FMath::VRandCone(BaseDirection, CurrentSpreadRad);
        }

        ShotDirections.Add(ShootDir);

        // -------------------------------------------------------------
        // DEBUG LINES: کێشانی هێڵ لە Muzzleـەوە بە ئاڕاستەی شوێنی لێدانەکە
        // -------------------------------------------------------------
#if ENABLE_DRAW_DEBUG
        FVector DebugEnd = MuzzleLoc + (ShootDir * CurrentWeaponData.MaxRange);
        DrawDebugLine(GetWorld(), MuzzleLoc, DebugEnd, (i == 0) ? FColor::Red : FColor::Orange, false, 2.0f, 0, 1.0f);
#endif
    }

    // ناردنی داتا بۆ سێرڤەر تەنها بە یەک RPC
    Server_ProcessShot(MuzzleLoc, ShotDirections);

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
    if (CurrentTime < (ServerNextAllowedFireTime - 0.05f)) return;

    // -------------------------------------------------------------
    // ANTI-CHEAT CHECK (Rule #4)
    // -------------------------------------------------------------
    // 1. ڕێگری لە هاکەر کە هەوڵبدات گوللە لە پشت دیوارەوە بتەقێنێت
    float DistSquared = FVector::DistSquared(OwnerCharacter->GetActorLocation(), MuzzleLocation);
    if (DistSquared > 30000.f) // ئەگەر Muzzle زیاتر لە ~170 یەکە دوور بوو لە یاریزانەکە
    {
        UE_LOG(LogTemp, Warning, TEXT("Anti-Cheat: Player %s tried to spoof Muzzle Location!"), *OwnerCharacter->GetName());
        return;
    }

    int32 MaxAllowedShots = OwnerCharacter->GetDoubleTap() ? 2 : 1;
    if (ShotDirections.Num() > MaxAllowedShots)
    {
        return; 
    }

    ServerNextAllowedFireTime = CurrentTime + GetEffectiveFireRate();

    // جێبەجێکردنی Trace بۆ هەر ئاڕاستەیەک کە نێردراوە
    for (const FVector_NetQuantizeNormal& Dir : ShotDirections)
    {
        ProcessShotLogic(MuzzleLocation, Dir);
    }
}

void ABaseWeapon::ProcessShotLogic(const FVector& TraceStart, const FVector& ShootDir)
{
    if (!HasAuthority() || !OwnerCharacter) return;

    FVector TraceEnd = TraceStart + (ShootDir * CurrentWeaponData.MaxRange);

    // ⚡ FIX: bTraceComplex لە true گۆڕدرا بۆ false.
    // سیستەمی Headshot/Legshot پشت بە Phys Material Override ـی Physics Asset دەبەستێت
    // (Simple Collision)، نەک بە Material ـی سەر Render Mesh (Complex Collision).
    // بە true بوونی، HitResult.PhysMaterial هەرگیز Override ـەکانی سەر/قاچ ناگرێتەوە،
    // بۆیە CalculateDamageBySurface هەمیشە دەکەوێتە بارودۆخی Default (= Body damage).
    FCollisionQueryParams Params(SCENE_QUERY_STAT(WeaponServerTrace), false, OwnerCharacter);
    Params.AddIgnoredActor(this);
    Params.bReturnPhysicalMaterial = true;

    AController* InstigatorController = OwnerCharacter->GetController();
    TSubclassOf<UDamageType> DamageTypeClass = UDamageType::StaticClass();

#if ENABLE_DRAW_DEBUG
    DrawDebugLine(GetWorld(), TraceStart, TraceEnd, FColor::Blue, false, 3.0f, 0, 1.5f);
#endif

    if (CurrentWeaponData.MaxZombiePenetration <= 1)
    {
        FHitResult HitResult;
        bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECollisionChannel::ECC_Bullet, Params);

        if (bHit && HitResult.GetActor())
        {
#if ENABLE_DRAW_DEBUG
            DrawDebugSphere(GetWorld(), HitResult.ImpactPoint, 6.0f, 12, FColor::Cyan, false, 3.0f);
#endif

            float FinalDamage = CalculateDamageBySurface(HitResult);

            UGameplayStatics::ApplyPointDamage(
                HitResult.GetActor(),
                FinalDamage,
                ShootDir,
                HitResult,
                InstigatorController,
                this,
                DamageTypeClass
            );
        }
    }
    else
    {
        TArray<FHitResult> HitResults;
        bool bHit = GetWorld()->LineTraceMultiByChannel(HitResults, TraceStart, TraceEnd, ECollisionChannel::ECC_Bullet, Params);

        if (bHit)
        {
            int32 PenCount = 0;
            for (const FHitResult& Hit : HitResults)
            {
                if (!Hit.GetActor()) continue;

#if ENABLE_DRAW_DEBUG
                DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 6.0f, 12, FColor::Cyan, false, 3.0f);
#endif

                float FinalDamage = CalculateDamageBySurface(Hit);

                UGameplayStatics::ApplyPointDamage(
                    Hit.GetActor(),
                    FinalDamage,
                    ShootDir,
                    Hit,
                    InstigatorController,
                    this,
                    DamageTypeClass
                );

                PenCount++;
                if (PenCount >= CurrentWeaponData.MaxZombiePenetration)
                {
                    break;
                }
            }
        }
    }
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
    if (!OwnerController || !OwnerCharacter) return;

    if (CurrentWeaponData.FireCameraShake) OwnerController->ClientStartCameraShake(CurrentWeaponData.FireCameraShake);

    if (CurrentWeaponData.RecoilPitchCurve)
    {
        float CurvePitchValue = CurrentWeaponData.RecoilPitchCurve->GetFloatValue(ShotsFiredInBurst);
        float RandomYaw = FMath::RandRange(-CurrentWeaponData.RecoilRandomness, CurrentWeaponData.RecoilRandomness);
        float Multiplier = (HamaComponent && HamaComponent->IsAiming()) ? CurrentWeaponData.AimRecoilMultiplier : 1.0f;

        TargetRecoilOffset = FRotator(CurvePitchValue * Multiplier, RandomYaw * Multiplier, 0.f);
        ShotsFiredInBurst++;
    }
}

void ABaseWeapon::ResetRecoil()
{
    ShotsFiredInBurst = 0;
    TargetRecoilOffset = FRotator::ZeroRotator;
    CurrentRecoilOffset = FRotator::ZeroRotator;
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
    return (ReserveAmmo < CurrentWeaponData.MaxReserveAmmo);
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
    float BaseTime = CurrentWeaponData.ReloadMontage ? CurrentWeaponData.ReloadMontage->GetPlayLength() : CurrentWeaponData.DefaultReloadTime;

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

    float FinalReloadTime = GetCalculatedReloadTime();

    if (CurrentWeaponData.ReloadMontage)
    {
        float PlayRate = (FinalReloadTime < CurrentWeaponData.ReloadMontage->GetPlayLength()) ? 2.0f : 1.0f;
        OwnerCharacter->PlayAnimMontage(CurrentWeaponData.ReloadMontage, PlayRate);
    }

    bool bIsEmpty = (CurrentAmmo <= 0);

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
            float PlayRate = (GetCalculatedReloadTime() < CurrentWeaponData.ReloadMontage->GetPlayLength()) ? 2.0f : 1.0f;
            AnimInstance->Montage_Play(CurrentWeaponData.ReloadMontage, PlayRate);
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