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

ABaseWeapon::ABaseWeapon()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    bReplicates = true;

    // چەک بۆ Weapon: 20/5 باشترە لە 40/10
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

    if (!OwnerCharacter || !OwnerCharacter->IsLocallyControlled() || !OwnerController) return;

    FRotator Previous = CurrentRecoilOffset;
    CurrentRecoilOffset = FMath::RInterpTo(CurrentRecoilOffset, TargetRecoilOffset, DeltaTime, 20.0f);

    float DeltaPitch = CurrentRecoilOffset.Pitch - Previous.Pitch;
    float DeltaYaw = CurrentRecoilOffset.Yaw - Previous.Yaw;

    OwnerController->AddPitchInput(-DeltaPitch);
    OwnerController->AddYawInput(DeltaYaw);

    if (CurrentRecoilOffset.Equals(TargetRecoilOffset, 0.01f))
    {
        SetActorTickEnabled(false);
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
    Damge = CurrentWeaponData.Damage;

    // Async Load — LoadSynchronous() کراش کردنەوەی BeginPlay
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

void ABaseWeapon::UpdateCachedReferences()
{
    OwnerCharacter = Cast<AHama>(GetOwner());
    if (!OwnerCharacter) return;
    if (!HamaComponent)
    {
        HamaComponent = OwnerCharacter->FindComponentByClass<UHamaComponent>();
    }
    if (!OwnerController)
    {
        OwnerController = Cast<APlayerController>(OwnerCharacter->GetController());
    }
}

void ABaseWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION(ABaseWeapon, CurrentAmmo, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(ABaseWeapon, ReserveAmmo, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(ABaseWeapon, Damge, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(ABaseWeapon, MaxAmmoInClip, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(ABaseWeapon, bIsReloading, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(ABaseWeapon, BurstCounter, COND_SkipOwner);
}

void ABaseWeapon::StartFire()
{
    if (CurrentAmmo <= 0 && ReserveAmmo <= 0) return;

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

        if (CurrentWeaponData.FireMode == EWeaponFireMode::Automatic
            || CurrentWeaponData.FireMode == EWeaponFireMode::Burst)
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
    // چاک کرا: Burst بوو Timer هەرگیز نەپاکدەبووەوە
    if (CurrentWeaponData.FireMode == EWeaponFireMode::Burst)
    {
        CurrentBurstShotsLeft = 0;
    }

    GetWorldTimerManager().ClearTimer(FireTimerHandle);
    ResetRecoil();
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

    if (CurrentAmmo <= 0)
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

    CurrentAmmo--;
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
        FeedbackParams.bIgnoreTimeDilation = false;
        FeedbackParams.Tag = FName("WeaponFire");
        OwnerController->ClientPlayForceFeedback(ShootForceFeedback, FeedbackParams);
    }

    FVector   CameraLoc;
    FRotator  CameraRot;
    OwnerController->GetPlayerViewPoint(CameraLoc, CameraRot);

    float Spread = CalculateBulletSpread();
    float SpreadInRadians = FMath::DegreesToRadians(Spread);

    int32         RandomSeed = FMath::Rand();
    FRandomStream WeaponStream(RandomSeed);
    WeaponStream.VRandCone(CameraRot.Vector(), SpreadInRadians);

    PlayWeaponEffects();
    ApplyRecoilAndCameraShake();

    ServerHandleFire(CameraLoc, CameraRot.Vector(), RandomSeed, SpreadInRadians);

    if (CurrentAmmo <= 0 || CurrentWeaponData.FireMode == EWeaponFireMode::Single)
    {
        StopFire();
        if (CurrentAmmo <= 0) Reload();
    }
}

void ABaseWeapon::ServerHandleFire_Implementation(
    FVector StartLocation, FVector CameraDirection,
    int32 RandomSeed, float SpreadInRadians)
{
    // ١. دڵنیابوونەوە لەوەی دەتوانێت تەقە بکات
    if (CurrentAmmo <= 0 || bIsReloading) return;

    AController* DamageInstigator = OwnerCharacter ? OwnerCharacter->GetController() : nullptr;

    if (OwnerCharacter && !OwnerCharacter->IsLocallyControlled())
    {
        CurrentAmmo--;
    }

    if (DamageInstigator)
    {
        FVector  ServerViewLoc;
        FRotator ServerViewRot;
        DamageInstigator->GetPlayerViewPoint(ServerViewLoc, ServerViewRot);
        if (FVector::DistSquared(StartLocation, ServerViewLoc) > FMath::Square(600.f))
        {
            StartLocation = ServerViewLoc;
        }
    }


    BurstCounter = (BurstCounter >= 255) ? 1 : BurstCounter + 1;

    // ٥. بەکارهێنانی هەمان Seed بۆ ئەوەی سێرڤەر و کڵایەنت هەمان بڵاوبوونەوە (Spread) دروست بکەن
    FRandomStream ServerStream(RandomSeed);
    FVector FinalFireDirection = ServerStream.VRandCone(CameraDirection, SpreadInRadians);
    FVector EndLocation = StartLocation + (FinalFireDirection * CurrentWeaponData.MaxRange);

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(GetOwner());
    Params.bReturnPhysicalMaterial = true;

    // ٧. فەنکشنی ناوخۆیی بۆ حیسابکردنی دیمەیج بەپێی جۆری ڕووی بەرکەوتن
    auto CalculateDamageBySurface = [this](const FHitResult& Hit) -> float
        {
            float ActualDamage = Damge; // دیمەیجی بنەڕەتی بەکاردێت بۆ جەستە (Body)
            if (Hit.PhysMaterial.IsValid())
            {
                EPhysicalSurface SurfaceType = UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());

                if (SurfaceType == EPhysicalSurface::SurfaceType1) // زۆرجار ئەمە سەرە (Head)
                {
                    ActualDamage *= CurrentWeaponData.HeadshotMultiplier;
                }
                else if (SurfaceType == EPhysicalSurface::SurfaceType2) // زۆرجار ئەمە قاچە (Leg)
                {
                    ActualDamage *= CurrentWeaponData.LegDamageMultiplier;
                }
            }
            return ActualDamage;
        };

    // ٨. لۆژیکی دیمەیجدان
    // ئەگەر چەکەکە توانای بڕینی زۆمبی نەبێت (فیشەکەکە بە ناو یەک زۆمبیدا تێپەڕ نەبێت بۆ ئەوەی دواتر)
    if (CurrentWeaponData.MaxZombiePenetration <= 1)
    {
        FHitResult Hit;
        bool bHasHit = GetWorld()->LineTraceSingleByChannel(Hit, StartLocation, EndLocation, ECC_Bullet, Params);

        if (bHasHit && Hit.GetActor())
        {
            float FinalDamage = CalculateDamageBySurface(Hit);
            UGameplayStatics::ApplyPointDamage(
                Hit.GetActor(), FinalDamage, FinalFireDirection,
                Hit, DamageInstigator, this, UDamageType::StaticClass()
            );
        }
    }
    else // ئەگەر چەکەکە وەک سنایپەر بێت و بتوانێت بەناو چەند زۆمبییەکدا تێپەڕێت
    {
        TArray<FHitResult> Hits;
        GetWorld()->LineTraceMultiByChannel(Hits, StartLocation, EndLocation, ECC_Bullet, Params);

        int32       PenetratedCount = 0;
        TSet<AActor*> HitActors;

        for (const FHitResult& SingleHit : Hits)
        {
            if (!SingleHit.GetActor() || HitActors.Contains(SingleHit.GetActor())) continue;

            float FinalDamage = CalculateDamageBySurface(SingleHit);
            UGameplayStatics::ApplyPointDamage(
                SingleHit.GetActor(), FinalDamage, FinalFireDirection,
                SingleHit, DamageInstigator, this, UDamageType::StaticClass()
            );

            HitActors.Add(SingleHit.GetActor());
            PenetratedCount++;

            if (PenetratedCount >= CurrentWeaponData.MaxZombiePenetration) break;
            if (SingleHit.bBlockingHit) break; // ئەگەر بەر دیوارێک کەوت، فیشەکەکە دەوەستێت
        }
    }
}

void ABaseWeapon::OnRep_BurstCounter()
{
    PlayWeaponEffects();
}

void ABaseWeapon::PlayWeaponEffects()
{
    // VFX و دەنگ لێرەدا زیاد بکە
}

void ABaseWeapon::Reload()
{
    if (ReserveAmmo <= 0
        || bIsReloading
        || CurrentAmmo == CurrentWeaponData.MaxAmmoInClip
        || !OwnerCharacter
        || !OwnerCharacter->IsLocallyControlled()) return;

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
    // چاک کرا: پێشتر if (HasAuthority()) return بوو
    // ListenServer هەرگیز Ammo نەجێبەجێدەکرد
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

    float ExactReloadTime = CurrentWeaponData.ReloadMontage
        ? CurrentWeaponData.ReloadMontage->GetPlayLength()
        : InReloadTime;

    float BufferTolerance = (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
        ? ExactReloadTime
        : FMath::Max(ExactReloadTime - 0.2f, 0.1f);

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

    if (OwnerCharacter && OwnerCharacter->IsLocallyControlled()
        && OwnerCharacter->bIsFireButtonHold)
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
                MeshComp->GetAnimInstance()->Montage_Stop(
                    0.2f, CurrentWeaponData.ReloadMontage
                );
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
            MeshComp->GetAnimInstance()->Montage_Stop(
                0.2f, CurrentWeaponData.ReloadMontage
            );
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

void ABaseWeapon::ApplyRecoilAndCameraShake()
{
    if (!OwnerCharacter || !OwnerCharacter->IsAimButtonHold()) return;

    // چاک کرا: پێشتر دوو جار IsAimButtonHold چێک دەکرد
    // و ShotsFiredInBurst لۆجیکەکەی هەڵە بوو
    if (ShotsFiredInBurst == 0)
    {
        ShotsFiredInBurst++;
        return;
    }

    float RandomPitch = FMath::RandRange(
        -CurrentWeaponData.RecoilRandomness, CurrentWeaponData.RecoilRandomness
    );
    float RandomYaw = FMath::RandRange(
        -CurrentWeaponData.RecoilRandomness, CurrentWeaponData.RecoilRandomness
    );

    float FinalPitch = (CurrentWeaponData.RecoilPitch + RandomPitch)
        * CurrentWeaponData.AimRecoilMultiplier;
    float FinalYaw = (CurrentWeaponData.RecoilYaw + RandomYaw)
        * CurrentWeaponData.AimRecoilMultiplier;

    TargetRecoilOffset.Pitch += FinalPitch;
    TargetRecoilOffset.Yaw += FinalYaw;
    ShotsFiredInBurst++;

    SetActorTickEnabled(true);
}

void ABaseWeapon::ResetRecoil()
{
    ShotsFiredInBurst = 0;
    TargetRecoilOffset = FRotator::ZeroRotator;
    CurrentRecoilOffset = FRotator::ZeroRotator;
    SetActorTickEnabled(false);
}