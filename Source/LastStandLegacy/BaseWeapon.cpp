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

    // ئەگەر گەیشتە ئامانجەکە، Tick بکوژێنەوە بۆ ئەوەی کایەکە خێرا بێت
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

    DOREPLIFETIME(ABaseWeapon, bInfiniteAmmo);
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

    CurrentAmmo = MaxAmmoInClip;
}

void ABaseWeapon::StartFire()
{
    if (GetWorldTimerManager().IsTimerActive(FireTimerHandle)) return;

    if (CurrentAmmo <= 0 && ReserveAmmo <= 0 && !bInfiniteAmmo) return;

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
            GetWorldTimerManager().SetTimer(
                FireTimerHandle, this, &ABaseWeapon::HandleFireLocal,
                CurrentWeaponData.FireRate, true
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
    if (HamaComponent && HamaComponent->bIsAiming)
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

    return CurrentSpread;
}

void ABaseWeapon::HandleFireLocal()
{
    if (!OwnerCharacter || !OwnerCharacter->IsLocallyControlled()) return;

    if (CurrentAmmo <= 0 && !bInfiniteAmmo)
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
        if(!bInfiniteAmmo) CurrentBurstShotsLeft--;
    }

    if (!OwnerController)
    {
        OwnerController = Cast<APlayerController>(OwnerCharacter->GetController());
        if (!OwnerController) return;
    }

    if (!bInfiniteAmmo)  CurrentAmmo--;
   
    float CurrentTime = GetWorld()->GetTimeSeconds();
    NextAllowedFireTime = CurrentTime + CurrentWeaponData.FireRate;

    if (HamaComponent && HamaComponent->bIsSprinting)
    {
        HamaComponent->StopSprinting();
    }

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

    FVector LaunchDirection = BaseDir;
    if (SpreadInRadians > 0.0f)
    {
        LaunchDirection = FMath::VRandCone(BaseDir, SpreadInRadians);
    }

    FVector EndLocation = CameraLoc + (LaunchDirection.GetSafeNormal() * CurrentWeaponData.MaxRange);
    FHitResult LocalHit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(OwnerCharacter);

#if !UE_BUILD_SHIPPING
    FColor TraceColor = FColor::Red;
#endif

    if (GetWorld()->LineTraceSingleByChannel(LocalHit, CameraLoc, EndLocation, ECC_Bullet, Params))
    {
        PlayLocalHitEffects(LocalHit);
#if !UE_BUILD_SHIPPING
        TraceColor = FColor::Green;
        DrawDebugSphere(GetWorld(), LocalHit.ImpactPoint, 10.0f, 12, FColor::Blue, false, 2.0f);
#endif
    }

#if !UE_BUILD_SHIPPING
    FVector LineEnd = LocalHit.bBlockingHit ? LocalHit.ImpactPoint : EndLocation;
    DrawDebugLine(GetWorld(), CameraLoc, LineEnd, TraceColor, false, 2.0f, 0, 2.0f);
#endif

    if (HasAuthority())
    {
        Server_FireRoutine();
    }

    if (CurrentAmmo <= 0 || CurrentWeaponData.FireMode == EWeaponFireMode::Single)
    {
        StopFire();
        if (CurrentAmmo <= 0) Reload();
    }
}

void ABaseWeapon::PlayLocalHitEffects(const FHitResult& LocalHit)
{
    // UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BloodEffect, LocalHit.ImpactPoint);
}

float ABaseWeapon::CalculateDamageBySurface(const FHitResult& Hit)
{
    float ActualDamage = Damage;
    if (Hit.PhysMaterial.IsValid())
    {
        EPhysicalSurface SurfaceType = UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());

        if (SurfaceType == EPhysicalSurface::SurfaceType1)
        {
            ActualDamage *= CurrentWeaponData.HeadshotMultiplier;
        }
        else if (SurfaceType == EPhysicalSurface::SurfaceType2)
        {
            ActualDamage *= CurrentWeaponData.LegDamageMultiplier;
        }
    }
    return ActualDamage;
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
        GetWorldTimerManager().SetTimer(
            ServerFireTimerHandle, this, &ABaseWeapon::Server_FireRoutine,
            CurrentWeaponData.FireRate, true
        );
    }
}

void ABaseWeapon::Server_StopFire_Implementation()
{
    GetWorldTimerManager().ClearTimer(ServerFireTimerHandle);
}

void ABaseWeapon::Server_FireRoutine()
{
    if (CurrentAmmo <= 0 || bIsReloading || !OwnerCharacter)
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
        if (!bInfiniteAmmo) CurrentBurstShotsLeft--;
    }

    float CurrentTime = GetWorld()->GetTimeSeconds();
    NextAllowedFireTime = CurrentTime + CurrentWeaponData.FireRate;

    if (HasAuthority() && !OwnerCharacter->IsLocallyControlled() && !bInfiniteAmmo)
    {
        CurrentAmmo--;
    }

    BurstCounter = (BurstCounter >= 255) ? 1 : BurstCounter + 1;
    AController* DamageInstigator = OwnerCharacter->GetController();

    FRotator ServerAimRotation = OwnerCharacter->GetBaseAimRotation();
    FVector BaseDir = ServerAimRotation.Vector();

    FVector StartLocation;
    if (DamageInstigator)
    {
        FRotator UnusedRot;
        DamageInstigator->GetPlayerViewPoint(StartLocation, UnusedRot);
    }
    else
    {
        StartLocation = OwnerCharacter->GetActorLocation();
    }

    float Spread = CalculateBulletSpread();
    float SpreadInRadians = FMath::DegreesToRadians(Spread);
    FVector LaunchDirection = BaseDir;
    if (SpreadInRadians > 0.0f)
    {
        LaunchDirection = FMath::VRandCone(BaseDir, SpreadInRadians);
    }

    FVector EndLocation = StartLocation + (LaunchDirection.GetSafeNormal() * CurrentWeaponData.MaxRange);
    FVector ShotDirection = LaunchDirection.GetSafeNormal();

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(OwnerCharacter);
    Params.bReturnPhysicalMaterial = true;

    if (CurrentWeaponData.MaxZombiePenetration <= 1)
    {
        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit, StartLocation, EndLocation, ECC_Bullet, Params))
        {
            if (Hit.GetActor())
            {
                float FinalDamage = CalculateDamageBySurface(Hit);
                UGameplayStatics::ApplyPointDamage(
                    Hit.GetActor(), FinalDamage, ShotDirection,
                    Hit, DamageInstigator, this, UDamageType::StaticClass()
                );
            }
        }
    }
    else
    {
        TArray<FHitResult> Hits;
        if (GetWorld()->LineTraceMultiByChannel(Hits, StartLocation, EndLocation, ECC_Bullet, Params))
        {
            int32 PenetratedCount = 0;
            TSet<AActor*> HitActors;

            for (const FHitResult& SingleHit : Hits)
            {
                if (!SingleHit.GetActor() || HitActors.Contains(SingleHit.GetActor())) continue;

                float FinalDamage = CalculateDamageBySurface(SingleHit);
                UGameplayStatics::ApplyPointDamage(
                    SingleHit.GetActor(), FinalDamage, ShotDirection,
                    SingleHit, DamageInstigator, this, UDamageType::StaticClass()
                );

                HitActors.Add(SingleHit.GetActor());
                PenetratedCount++;

                if (PenetratedCount >= CurrentWeaponData.MaxZombiePenetration || SingleHit.bBlockingHit) break;
            }
        }
    }
}

// -----------------------------------------------------

void ABaseWeapon::ApplyRecoilAndCameraShake()
{
    if (!OwnerController || !OwnerCharacter) return;

    if (CurrentWeaponData.FireCameraShake)
    {
        OwnerController->ClientStartCameraShake(CurrentWeaponData.FireCameraShake);
    }

    // گۆڕدرا بۆ پشکنینی کێرڤە فڵۆتەکە
    if (CurrentWeaponData.RecoilPitchCurve)
    {
        // خوێندنەوەی تاقە بەهاکەی ناو کێرڤەکە (کە تەنها بەرزبوونەوەیە)
        float CurvePitchValue = CurrentWeaponData.RecoilPitchCurve->GetFloatValue(ShotsFiredInBurst);

        float RandomYaw = FMath::RandRange(-CurrentWeaponData.RecoilRandomness, CurrentWeaponData.RecoilRandomness);

        float Multiplier = (HamaComponent && HamaComponent->bIsAiming) ? CurrentWeaponData.AimRecoilMultiplier : 1.0f;

        // خاوێنترین و سادەترین لۆجیک
        float TargetPitch = CurvePitchValue * Multiplier;
        float TargetYaw = RandomYaw * Multiplier;

        TargetRecoilOffset = FRotator(TargetPitch, TargetYaw, 0.f);

        SetActorTickEnabled(true);

        ShotsFiredInBurst++;
    }
}

void ABaseWeapon::ResetRecoil()
{
    ShotsFiredInBurst = 0;
    TargetRecoilOffset = FRotator::ZeroRotator;
    CurrentRecoilOffset = FRotator::ZeroRotator;
    SetActorTickEnabled(false);
}

void ABaseWeapon::OnRep_BurstCounter()
{
    PlayWeaponEffects();
}

void ABaseWeapon::OnRep_InfiniteAmmo()
{
}

void ABaseWeapon::PlayWeaponEffects()
{
    // VFX و دەنگ
}

void ABaseWeapon::Reload()
{
    if (ReserveAmmo <= 0 || bIsReloading || CurrentAmmo == CurrentWeaponData.MaxAmmoInClip || !OwnerCharacter || !OwnerCharacter->IsLocallyControlled()) return;

    bIsReloading = true;
    float ReloadTimeToUse = CurrentWeaponData.DefaultReloadTime;

    if (CurrentWeaponData.ReloadMontage)
    {
        OwnerCharacter->PlayAnimMontage(CurrentWeaponData.ReloadMontage);
        ReloadTimeToUse = CurrentWeaponData.ReloadMontage->GetPlayLength();
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

    float ExactReloadTime = CurrentWeaponData.ReloadMontage ? CurrentWeaponData.ReloadMontage->GetPlayLength() : InReloadTime;
    float BufferTolerance = (OwnerCharacter && OwnerCharacter->IsLocallyControlled()) ? ExactReloadTime : FMath::Max(ExactReloadTime - 0.2f, 0.1f);

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

    if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
    {
        bIsReloading = false;

        USkeletalMeshComponent* MeshComp = OwnerCharacter->GetMesh();
        if (MeshComp && MeshComp->GetAnimInstance())
        {
            MeshComp->GetAnimInstance()->Montage_Stop(0.2f, CurrentWeaponData.ReloadMontage);
        }

        if (!HasAuthority()) Server_CancelReload();
    }

    if (HasAuthority())
    {
        bIsReloading = false;

        if (OwnerCharacter && !OwnerCharacter->IsLocallyControlled())
        {
            USkeletalMeshComponent* MeshComp = OwnerCharacter->GetMesh();
            if (MeshComp && MeshComp->GetAnimInstance())
            {
                MeshComp->GetAnimInstance()->Montage_Stop(0.2f, CurrentWeaponData.ReloadMontage);
            }
        }
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
        if (MeshComp && MeshComp->GetAnimInstance())
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
            AnimInstance->Montage_Play(CurrentWeaponData.ReloadMontage);
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