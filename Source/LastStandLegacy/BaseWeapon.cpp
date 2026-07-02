#include "BaseWeapon.h"
#include "Hama.h"
#include "HamaComponent.h"
#include "Net/UnrealNetwork.h"
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

    // 15.0f خێرایی نەرمکردنەوەکەیە (Interpolation Speed). تا بچووکتر بێت نەرمتر دەبێت.
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

    DOREPLIFETIME_CONDITION(ABaseWeapon, CurrentAmmo, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(ABaseWeapon, ReserveAmmo, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(ABaseWeapon, Damage, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(ABaseWeapon, MaxAmmoInClip, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(ABaseWeapon, bIsReloading, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(ABaseWeapon, BurstCounter, COND_SkipOwner);
}

void ABaseWeapon::ServerUpgradeWeapon_PackAPunch_Implementation()
{
    if (!HasAuthority()) return;

    Damage *= 2.f;
    MaxAmmoInClip = FMath::RoundToInt(MaxAmmoInClip * 1.5f);
    ReserveAmmo = CurrentWeaponData.MaxReserveAmmo * 2;
    CurrentWeaponData.MaxReserveAmmo = ReserveAmmo;

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

    if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
    {
        OnAmmoChanged.ExecuteIfBound(CurrentAmmo, ReserveAmmo);
    }

    if (bWasEmpty)
    {
        Client_ForceReload();
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
        StopFire();
        Reload();
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
                    float FinalDamage = CalculateDamageBySurface(LocalHit);
                    Server_ApplyDamage(LocalHit.GetActor(), FinalDamage, ShotDirection, LocalHit);
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

                    float FinalDamage = CalculateDamageBySurface(SingleHit);
                    Server_ApplyDamage(SingleHit.GetActor(), FinalDamage, ShotDirection, SingleHit);

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
        }
    }

    BurstCounter = (BurstCounter >= 255) ? 1 : BurstCounter + 1;
}

void ABaseWeapon::Server_ApplyDamage_Implementation(AActor* HitActor, float DamageToApply, FVector ShotDirection, FHitResult HitInfo)
{
    if (!HitActor || !OwnerCharacter) return;

    AController* DamageInstigator = OwnerCharacter->GetController();

    UGameplayStatics::ApplyPointDamage(
        HitActor, DamageToApply, ShotDirection,
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
    return (CurrentAmmo < MaxAmmoInClip) || (ReserveAmmo < CurrentWeaponData.MaxReserveAmmo);
}

void ABaseWeapon::OnRep_BurstCounter()
{
    PlayWeaponEffects();
}

void ABaseWeapon::PlayWeaponEffects()
{
    // VFX و دەنگ
}

void ABaseWeapon::Client_ForceReload_Implementation()
{
    Reload();
}

void ABaseWeapon::Reload()
{
    if (ReserveAmmo <= 0 || CurrentAmmo >= MaxAmmoInClip || bIsReloading || CurrentAmmo == CurrentWeaponData.MaxAmmoInClip || !OwnerCharacter || !OwnerCharacter->IsLocallyControlled()) return;

    bIsReloading = true;
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

    GetWorldTimerManager().SetTimer(
        ReloadTimerHandle, this, &ABaseWeapon::Local_ReloadComplete,
        ReloadTimeToUse, false
    );

    if (HasAuthority())
    {
        ServerReload_Implementation(ReloadTimeToUse);
    }
    else
    {
        ServerReload(ReloadTimeToUse);
    }
}

void ABaseWeapon::Local_ReloadComplete()
{
    if (!OwnerCharacter || !OwnerCharacter->IsLocallyControlled()) return;

    int32 AmmoNeeded = CurrentWeaponData.MaxAmmoInClip - CurrentAmmo;
    int32 AmmoToMove = FMath::Min(AmmoNeeded, ReserveAmmo);

    CurrentAmmo += AmmoToMove;
    ReserveAmmo -= AmmoToMove;
    bIsReloading = false;

    OnAmmoChanged.ExecuteIfBound(CurrentAmmo, ReserveAmmo);

    if (OwnerCharacter->bIsFireButtonHold)
    {
        StartFire();
    }
}

void ABaseWeapon::ServerReload_Implementation(float InReloadTime)
{
    bIsReloading = true;

    if (OwnerCharacter && !OwnerCharacter->IsLocallyControlled())
    {
        OnRep_Reload();
    }

    float FinalReloadTime = CurrentWeaponData.ReloadMontage ? CurrentWeaponData.ReloadMontage->GetPlayLength() : InReloadTime;
    ALastStandLegacyGameState* GS = GetGameStateCache();
    if ((GS && GS->IsTeamAdrenalineActive()) || (OwnerCharacter && OwnerCharacter->HasFastHands()))
    {
        FinalReloadTime /= 2.0f;
    }

    float BufferTolerance = FMath::Max(FinalReloadTime - 0.1f, 0.1f);

    GetWorldTimerManager().SetTimer(
        ReloadTimerHandle, this, &ABaseWeapon::Server_ReloadComplete,
        BufferTolerance, false
    );
}

void ABaseWeapon::Server_ReloadComplete()
{
    int32 AmmoNeeded = CurrentWeaponData.MaxAmmoInClip - CurrentAmmo;
    int32 AmmoToMove = FMath::Min(AmmoNeeded, ReserveAmmo);

    CurrentAmmo += AmmoToMove;
    ReserveAmmo -= AmmoToMove;
    bIsReloading = false;

    if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
    {
        OnAmmoChanged.ExecuteIfBound(CurrentAmmo, ReserveAmmo);
    }

    if (OwnerCharacter && !OwnerCharacter->IsLocallyControlled())
    {
        OnRep_Reload();
    }

    if (OwnerCharacter && OwnerCharacter->IsLocallyControlled() && OwnerCharacter->bIsFireButtonHold)
    {
        StartFire();
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
    }

    if (HasAuthority() && OwnerCharacter && !OwnerCharacter->IsLocallyControlled())
    {
        OnRep_Reload();
    }
}

void ABaseWeapon::Server_CancelReload_Implementation()
{
    bIsReloading = false;
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
    OnAmmoChanged.ExecuteIfBound(CurrentAmmo, ReserveAmmo);
}

void ABaseWeapon::OnRep_ReserveAmmo()
{
    OnAmmoChanged.ExecuteIfBound(CurrentAmmo, ReserveAmmo);
}