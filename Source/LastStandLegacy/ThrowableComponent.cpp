#include "ThrowableComponent.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Hama.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"

UThrowableComponent::UThrowableComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);

    ChargeState = EThrowChargeState::Idle;
    bIsMonkeyThrow = false;
    bIsThrowingLocal = false;
    LastThrowTime = -100.0f;
    ThrowCooldown = 1.0f;

    MaxGrenadeCount = 5;
    CurrentGrenadeCount = 3;

    MaxMonkeyCount = 3;
    CurrentMonkeyCount = 0;

    MaxGrenadeCookTime = 3.5f;
    ThrowableHandSocketName = TEXT("GrenadeHandSocket");
}

void UThrowableComponent::BeginPlay()
{
    Super::BeginPlay();
    CharacterOwner = Cast<AHama>(GetOwner());

    if (GrenadeThrowMontage)
    {
        int32 SectionIndex = GrenadeThrowMontage->GetSectionIndex(ReleaseSectionName);
        ThrowCooldown = (SectionIndex != INDEX_NONE) ? GrenadeThrowMontage->GetSectionLength(SectionIndex) : GrenadeThrowMontage->GetPlayLength();
    }
}

void UThrowableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams RepParams;
    RepParams.bIsPushBased = true;

    RepParams.Condition = COND_OwnerOnly;
    DOREPLIFETIME_WITH_PARAMS(UThrowableComponent, CurrentGrenadeCount, RepParams);
    DOREPLIFETIME_WITH_PARAMS(UThrowableComponent, CurrentMonkeyCount, RepParams);

    RepParams.Condition = COND_SkipOwner;
    DOREPLIFETIME_WITH_PARAMS(UThrowableComponent, ChargeState, RepParams);
    DOREPLIFETIME_WITH_PARAMS(UThrowableComponent, bIsMonkeyThrow, RepParams);
}

void UThrowableComponent::StartGrenadeCharge() { Internal_StartCharge(GrenadeClass, CurrentGrenadeCount, GrenadeThrowMontage, false); }
void UThrowableComponent::StartMonkeyCharge() { Internal_StartCharge(MonkeyClass, CurrentMonkeyCount, MonkeyThrowMontage, true); }

void UThrowableComponent::Internal_StartCharge(TSubclassOf<AActor> ThrowableClass, int32 CurrentCount, UAnimMontage* MontageToPlay, bool bIsMonkey)
{
    if (!CharacterOwner || !ThrowableClass || CurrentCount <= 0 || IsCharging() || IsThrowingInProcess())
    {
        return;
    }

    const float CurrentTime = GetWorld()->GetTimeSeconds();

    if (CurrentTime - LastThrowTime < ThrowCooldown)
    {
        return;
    }

    bIsThrowingLocal = true;
    bIsMonkeyThrow = bIsMonkey;
    ChargeState = EThrowChargeState::Charging;

    HandleChargeStateChanged();

    if (!GetOwner()->HasAuthority())
    {
        Server_StartCharge(bIsMonkey);
    }
    else
    {
        MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, bIsMonkeyThrow, this);
        MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, ChargeState, this);

        if (!bIsMonkeyThrow)
        {
            GetWorld()->GetTimerManager().SetTimer(
                TimerHandle_CookExplosion,
                this,
                &UThrowableComponent::Server_OnGrenadeCookExpired,
                MaxGrenadeCookTime,
                false
            );
        }
    }
}

void UThrowableComponent::Server_StartCharge_Implementation(bool bIsMonkey)
{
    const float CurrentTime = GetWorld()->GetTimeSeconds();
    if (!CharacterOwner || (CurrentTime - LastThrowTime < (ThrowCooldown - 0.05f)) || ChargeState != EThrowChargeState::Idle)
    {
        Client_ResetThrowState();
        return;
    }

    bIsThrowingLocal = true;
    bIsMonkeyThrow = bIsMonkey;
    ChargeState = EThrowChargeState::Charging;

    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, bIsMonkeyThrow, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, ChargeState, this);

    HandleChargeStateChanged();

    if (!bIsMonkeyThrow)
    {
        GetWorld()->GetTimerManager().SetTimer(
            TimerHandle_CookExplosion,
            this,
            &UThrowableComponent::Server_OnGrenadeCookExpired,
            MaxGrenadeCookTime,
            false
        );
    }
}

void UThrowableComponent::ReleaseGrenadeThrow() { Internal_ReleaseThrow(GrenadeClass, CurrentGrenadeCount, GrenadeThrowMontage, false); }
void UThrowableComponent::ReleaseMonkeyThrow() { Internal_ReleaseThrow(MonkeyClass, CurrentMonkeyCount, MonkeyThrowMontage, true); }

void UThrowableComponent::Internal_ReleaseThrow(TSubclassOf<AActor> ThrowableClass, int32 CurrentCount, UAnimMontage* MontageToPlay, bool bIsMonkey)
{
    if (!CharacterOwner || ChargeState != EThrowChargeState::Charging)
    {
        return;
    }

    ChargeState = EThrowChargeState::Released;
    HandleChargeStateChanged();

    FVector AimDir = GetCameraAimDirection();

    Server_ExecuteThrow(AimDir, bIsMonkey);

    if (!GetOwner()->HasAuthority())
    {
        LastThrowTime = GetWorld()->GetTimeSeconds();
    }
}

void UThrowableComponent::Server_ExecuteThrow_Implementation(FVector_NetQuantizeNormal LaunchDirection, bool bIsMonkey)
{
    const bool bActualIsMonkey = bIsMonkeyThrow;

    TSubclassOf<AActor> TargetClass = bActualIsMonkey ? MonkeyClass : GrenadeClass;
    int32& TargetCount = bActualIsMonkey ? CurrentMonkeyCount : CurrentGrenadeCount;

    const float CurrentTime = GetWorld()->GetTimeSeconds();
    const bool bIsCooldownActive = (CurrentTime - LastThrowTime) < (ThrowCooldown - 0.05f);

    const bool bValidState = (ChargeState == EThrowChargeState::Charging) ||
        (CharacterOwner && CharacterOwner->HasAuthority() && ChargeState == EThrowChargeState::Released);

    if (!CharacterOwner || !TargetClass || TargetCount <= 0 || !bValidState || bIsCooldownActive)
    {
        ResetThrowState_Server();
        Client_ResetThrowState();
        return;
    }

    if (!bActualIsMonkey && GetWorld()->GetTimerManager().IsTimerActive(TimerHandle_CookExplosion))
    {
        GetWorld()->GetTimerManager().ClearTimer(TimerHandle_CookExplosion);
    }

    LastThrowTime = CurrentTime;
    ChargeState = EThrowChargeState::Released;

    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, ChargeState, this);
    HandleChargeStateChanged();

    FVector ServerAimDir = CharacterOwner->GetBaseAimRotation().Vector();
    FVector ValidatedAimDir = LaunchDirection.GetSafeNormal();

    if (FVector::DotProduct(ValidatedAimDir, ServerAimDir) < 0.7f)
    {
        ValidatedAimDir = ServerAimDir;
    }

    FVector TargetPoint = GetCrosshairTargetPoint(ValidatedAimDir);
    FVector PawnViewLoc = CharacterOwner->GetPawnViewLocation();
    FVector StartLoc = PawnViewLoc + (ValidatedAimDir * 40.0f);

    FVector FinalSpawnLoc;
    GetSafeSpawnLocation(PawnViewLoc, StartLoc, FinalSpawnLoc);
    FVector TrueLaunchDir = (TargetPoint - FinalSpawnLoc).GetSafeNormal();

    FRotator SpawnRotation = TrueLaunchDir.Rotation();
    FTransform SpawnTransform(SpawnRotation, FinalSpawnLoc);

    AActor* SpawnedThrowable = GetWorld()->SpawnActorDeferred<AActor>(
        TargetClass, SpawnTransform, CharacterOwner, CharacterOwner, ESpawnActorCollisionHandlingMethod::AlwaysSpawn
    );

    if (SpawnedThrowable)
    {
        if (UProjectileMovementComponent* ProjComp = SpawnedThrowable->FindComponentByClass<UProjectileMovementComponent>())
        {
            ProjComp->bInitialVelocityInLocalSpace = false;
            ProjComp->MaxSpeed = FMath::Max(ProjComp->MaxSpeed, ThrowImpulseStrength);
            ProjComp->Velocity = TrueLaunchDir * ThrowImpulseStrength;
        }

        UGameplayStatics::FinishSpawningActor(SpawnedThrowable, SpawnTransform);

        TargetCount--;
        if (bActualIsMonkey)
        {
            MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentMonkeyCount, this);
            OnRep_MonkeyCount();
        }
        else
        {
            MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentGrenadeCount, this);
            OnRep_GrenadeCount();
        }
    }
    else
    {
        ResetThrowState_Server();
        Client_ResetThrowState();
        return;
    }

    GetWorld()->GetTimerManager().SetTimer(
        TimerHandle_ResetThrowState, this, &UThrowableComponent::ResetThrowState_Server, ThrowCooldown, false
    );
}

void UThrowableComponent::Server_OnGrenadeCookExpired()
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority()) return;

    FVector HandLocation = CharacterOwner->GetMesh() ?
        CharacterOwner->GetMesh()->GetSocketLocation(ThrowableHandSocketName) : CharacterOwner->GetActorLocation();

    if (GrenadeClass)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = CharacterOwner;
        SpawnParams.Instigator = CharacterOwner;

        GetWorld()->SpawnActor<AActor>(GrenadeClass, HandLocation, FRotator::ZeroRotator, SpawnParams);
    }

    CurrentGrenadeCount = FMath::Max(0, CurrentGrenadeCount - 1);
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentGrenadeCount, this);
    OnRep_GrenadeCount();

    ResetThrowState_Server();
    Client_ResetThrowState();
}

void UThrowableComponent::UnlockAndRefillMonkeyBomb(TSubclassOf<AActor> NewMonkeyClass, int32 Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (NewMonkeyClass) MonkeyClass = NewMonkeyClass;

    CurrentMonkeyCount = FMath::Clamp(Amount, 0, MaxMonkeyCount);
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentMonkeyCount, this);
    OnRep_MonkeyCount();
}

void UThrowableComponent::RefillMonkeyToMax()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !MonkeyClass) return;
    CurrentMonkeyCount = MaxMonkeyCount;
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentMonkeyCount, this);
    OnRep_MonkeyCount();
}

void UThrowableComponent::RefillGrenadesToMax()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    CurrentGrenadeCount = MaxGrenadeCount;
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentGrenadeCount, this);
    OnRep_GrenadeCount();
}

void UThrowableComponent::OnRep_GrenadeCount() { OnThrowableCountChanged.Broadcast(CurrentMonkeyCount, CurrentGrenadeCount); }
void UThrowableComponent::OnRep_MonkeyCount() { OnThrowableCountChanged.Broadcast(CurrentMonkeyCount, CurrentGrenadeCount); }

void UThrowableComponent::ResetThrowState_Server()
{
    bIsThrowingLocal = false;
    ChargeState = EThrowChargeState::Idle;

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(TimerHandle_CookExplosion);
    }

    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, ChargeState, this);
    SetWeaponHidden(false);

    Client_ResetThrowState();
}

void UThrowableComponent::Client_ResetThrowState_Implementation()
{
    bIsThrowingLocal = false;
    ChargeState = EThrowChargeState::Idle;

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(TimerHandle_CookExplosion);
    }

    if (CharacterOwner && CharacterOwner->GetMesh())
    {
        if (UAnimInstance* AnimInstance = CharacterOwner->GetMesh()->GetAnimInstance())
        {
            UAnimMontage* ActiveMontage = bIsMonkeyThrow ? MonkeyThrowMontage : GrenadeThrowMontage;
            if (ActiveMontage)
            {
                AnimInstance->Montage_Stop(0.2f, ActiveMontage);
            }
        }
    }

    SetWeaponHidden(false);
}

void UThrowableComponent::OnThrowMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    bIsThrowingLocal = false;
    ChargeState = EThrowChargeState::Idle;
    SetWeaponHidden(false);
}

void UThrowableComponent::OnRep_ChargeState()
{
    HandleChargeStateChanged();
}

void UThrowableComponent::HandleChargeStateChanged()
{
    UAnimMontage* MontageToPlay = bIsMonkeyThrow ? MonkeyThrowMontage : GrenadeThrowMontage;
    if (!CharacterOwner || !MontageToPlay || !CharacterOwner->GetMesh()) return;

    UAnimInstance* AnimInstance = CharacterOwner->GetMesh()->GetAnimInstance();
    if (!AnimInstance) return;

    switch (ChargeState)
    {
    case EThrowChargeState::Charging:
    {
        AnimInstance->Montage_Play(MontageToPlay);
        AnimInstance->Montage_JumpToSection(HoldSectionName, MontageToPlay);
        AnimInstance->Montage_SetPlayRate(MontageToPlay, 0.0f);

        FOnMontageEnded EndedDelegate;
        EndedDelegate.BindUObject(this, &UThrowableComponent::OnThrowMontageEnded);
        AnimInstance->Montage_SetEndDelegate(EndedDelegate, MontageToPlay);

        SetWeaponHidden(true);
        SpawnAndAttachHeldVisual();
        break;
    }
    case EThrowChargeState::Released:
    {
        AnimInstance->Montage_SetPlayRate(MontageToPlay, 1.0f);
        AnimInstance->Montage_JumpToSection(ReleaseSectionName, MontageToPlay);
        SetWeaponHidden(false);
        DestroyHeldVisual();
        break;
    }
    case EThrowChargeState::Idle:
    default:
        SetWeaponHidden(false);
        DestroyHeldVisual();
        break;
    }
}

void UThrowableComponent::SpawnAndAttachHeldVisual()
{
    DestroyHeldVisual();

    TSubclassOf<AActor> TargetClass = bIsMonkeyThrow ? MonkeyClass : GrenadeClass;
    if (!TargetClass || !CharacterOwner || !CharacterOwner->GetMesh()) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = CharacterOwner;
    SpawnParams.Instigator = CharacterOwner;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    HeldThrowableVisualActor = GetWorld()->SpawnActor<AActor>(TargetClass, SpawnParams);
    if (HeldThrowableVisualActor)
    {
        HeldThrowableVisualActor->SetReplicates(false);
        HeldThrowableVisualActor->SetActorEnableCollision(false);

        if (UProjectileMovementComponent* ProjComp = HeldThrowableVisualActor->FindComponentByClass<UProjectileMovementComponent>())
        {
            ProjComp->Deactivate();
        }

        FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, true);
        HeldThrowableVisualActor->AttachToComponent(CharacterOwner->GetMesh(), AttachRules, ThrowableHandSocketName);
    }
}

void UThrowableComponent::DestroyHeldVisual()
{
    if (HeldThrowableVisualActor)
    {
        HeldThrowableVisualActor->Destroy();
        HeldThrowableVisualActor = nullptr;
    }
}

void UThrowableComponent::SetWeaponHidden(bool bHidden)
{
    if (IsValid(CharacterOwner) && IsValid(CharacterOwner->CurrentWeapon))
    {
        CharacterOwner->CurrentWeapon->SetActorHiddenInGame(bHidden);
    }
}

FVector UThrowableComponent::GetCameraAimDirection() const
{
    if (!CharacterOwner) return FVector::ForwardVector;

    if (APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController()))
    {
        FVector CameraLoc;
        FRotator CameraRot;
        PC->GetPlayerViewPoint(CameraLoc, CameraRot);
        return CameraRot.Vector();
    }

    return CharacterOwner->GetBaseAimRotation().Vector();
}

FVector UThrowableComponent::GetCrosshairTargetPoint(const FVector& AimDir) const
{
    if (!CharacterOwner) return FVector::ZeroVector;

    FVector CameraLoc;
    FRotator CameraRot;

    if (APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController()))
    {
        PC->GetPlayerViewPoint(CameraLoc, CameraRot);
    }
    else
    {
        CameraLoc = CharacterOwner->GetPawnViewLocation();
    }

    FVector TraceEnd = CameraLoc + (AimDir * 10000.0f);
    FHitResult Hit;
    FCollisionQueryParams TraceParams;
    TraceParams.AddIgnoredActor(CharacterOwner);

    if (GetWorld() && GetWorld()->LineTraceSingleByChannel(Hit, CameraLoc, TraceEnd, ECC_WorldStatic, TraceParams))
    {
        return Hit.ImpactPoint;
    }

    return TraceEnd;
}

void UThrowableComponent::GetSafeSpawnLocation(const FVector& StartLoc, const FVector& TargetLoc, FVector& OutSpawnLoc) const
{
    if (!GetWorld() || !CharacterOwner)
    {
        OutSpawnLoc = TargetLoc;
        return;
    }

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(CharacterOwner);

    if (GetWorld()->LineTraceSingleByChannel(HitResult, StartLoc, TargetLoc, ECC_WorldStatic, QueryParams))
    {
        OutSpawnLoc = HitResult.Location - (TargetLoc - StartLoc).GetSafeNormal() * 10.0f;
    }
    else
    {
        OutSpawnLoc = TargetLoc;
    }
}