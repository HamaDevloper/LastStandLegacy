#include "ThrowableComponent.h"
#include "Hama.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"

// NOTE: this file expects a new member on UThrowableComponent, declared in
// ThrowableComponent.h alongside the other members (e.g. under CurrentThrowableCount):
//     FVector CachedThrowStartLoc;
// It caches the throw origin at charge-start instead of resampling the hand
// socket at release, so a long hold can't drag the spawn point into an
// unexpected pose (the "teleport" bug).

UThrowableComponent::UThrowableComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
    bIsCharging = false;
    bIsThrowingInProcess = false;
    LastThrowTime = -100.0f;
}

void UThrowableComponent::BeginPlay()
{
    Super::BeginPlay();
    CharacterOwner = Cast<AHama>(GetOwner());

    if (ThrowMontage)
    {
        int32 SectionIndex = ThrowMontage->GetSectionIndex(ReleaseSectionName);
        if (SectionIndex != INDEX_NONE)
        {
            ThrowCooldown = ThrowMontage->GetSectionLength(SectionIndex);
        }
        else
        {
            ThrowCooldown = ThrowMontage->GetPlayLength();
        }
    }
}

void UThrowableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Params;
    Params.bIsPushBased = true;

    DOREPLIFETIME_WITH_PARAMS(UThrowableComponent, CurrentThrowableCount, Params);
    DOREPLIFETIME_WITH_PARAMS(UThrowableComponent, bIsCharging, Params);
    DOREPLIFETIME_WITH_PARAMS(UThrowableComponent, bIsThrowingInProcess, Params);
}

FVector UThrowableComponent::GetCameraAimDirection() const
{
    if (!CharacterOwner) return FVector::ForwardVector;

    APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
    if (PC)
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

    APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
    if (PC)
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

    if (GetWorld() && GetWorld()->LineTraceSingleByChannel(Hit, CameraLoc, TraceEnd, ECC_Visibility, TraceParams))
    {
        return Hit.ImpactPoint;
    }

    return TraceEnd;
}

void UThrowableComponent::StartThrowCharge()
{
    if (CharacterOwner && CharacterOwner->IsSwappingWeapon()) return;
    if (!CharacterOwner || !ThrowableClass || CurrentThrowableCount <= 0 || bIsCharging || bIsThrowingInProcess) return;

    const float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastThrowTime < ThrowCooldown) return;

    if (CharacterOwner->GetMesh())
    {
        if (UAnimInstance* AnimInstance = CharacterOwner->GetMesh()->GetAnimInstance())
        {
            if (AnimInstance->Montage_IsPlaying(ThrowMontage)) return;
        }
    }

    bIsCharging = true;
    bIsThrowingInProcess = true;

    SetWeaponVisibility(true);

    if (ThrowMontage && CharacterOwner->GetMesh())
    {
        if (UAnimInstance* AnimInstance = CharacterOwner->GetMesh()->GetAnimInstance())
        {
            AnimInstance->Montage_Play(ThrowMontage);
            AnimInstance->Montage_JumpToSection(HoldSectionName, ThrowMontage);
            AnimInstance->Montage_SetPlayRate(ThrowMontage, 0.0f);
        }
    }

    if (!GetOwner()->HasAuthority())
    {
        Server_StartThrowCharge();
    }
}

void UThrowableComponent::Server_StartThrowCharge_Implementation()
{
    const float CurrentTime = GetWorld()->GetTimeSeconds();
    const float NetTolerance = 0.05f;

    if (!CharacterOwner || CharacterOwner->IsSwappingWeapon() || CurrentThrowableCount <= 0 ||
        (CurrentTime - LastThrowTime < (ThrowCooldown - NetTolerance)) || bIsThrowingInProcess)
    {
        if (!bIsThrowingInProcess)
        {
            ResetThrowState_Server();
        }
        Client_ResetThrowState();
        return;
    }

    bIsCharging = true;
    bIsThrowingInProcess = true;
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, bIsCharging, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, bIsThrowingInProcess, this);

    if (CharacterOwner->GetMesh() && CharacterOwner->GetMesh()->DoesSocketExist(HandSocketName))
    {
        CachedThrowStartLoc = CharacterOwner->GetMesh()->GetSocketLocation(HandSocketName);
    }
    else
    {
        CachedThrowStartLoc = CharacterOwner->GetPawnViewLocation();
    }

    if (GetNetMode() == NM_ListenServer && !CharacterOwner->IsLocallyControlled())
    {
        OnRep_IsCharging();
    }
}

void UThrowableComponent::ReleaseThrow()
{
    if (!CharacterOwner || !bIsCharging) return;

    bIsCharging = false;

    LastThrowTime = GetWorld()->GetTimeSeconds();

    if (ThrowMontage && CharacterOwner->GetMesh())
    {
        if (UAnimInstance* AnimInstance = CharacterOwner->GetMesh()->GetAnimInstance())
        {
            AnimInstance->Montage_SetPlayRate(ThrowMontage, 1.0f);
            AnimInstance->Montage_JumpToSection(ReleaseSectionName, ThrowMontage);

            FOnMontageEnded EndedDelegate;
            EndedDelegate.BindUObject(this, &UThrowableComponent::OnThrowMontageEnded);
            AnimInstance->Montage_SetEndDelegate(EndedDelegate, ThrowMontage);
        }
    }

    FVector AimDir = GetCameraAimDirection();
    Server_ReleaseThrow(AimDir);
}

void UThrowableComponent::Server_ReleaseThrow_Implementation(FVector_NetQuantizeNormal LaunchDirection)
{
    const float CurrentTime = GetWorld()->GetTimeSeconds();

    if (!CharacterOwner || !ThrowableClass || CurrentThrowableCount <= 0 || !bIsCharging)
    {
        ResetThrowState_Server();
        Client_ResetThrowState();
        return;
    }

    LastThrowTime = CurrentTime;
    bIsCharging = false;
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, bIsCharging, this);

    if (GetNetMode() == NM_ListenServer && !CharacterOwner->IsLocallyControlled())
    {
        OnRep_IsCharging();
    }

    FVector ValidatedAimDir = LaunchDirection.GetSafeNormal();
    FVector TargetPoint = GetCrosshairTargetPoint(ValidatedAimDir);

    FVector StartLoc;
    if (CharacterOwner->GetMesh() && CharacterOwner->GetMesh()->DoesSocketExist(HandSocketName))
    {
        StartLoc = CharacterOwner->GetMesh()->GetSocketLocation(HandSocketName);
    }
    else
    {
        StartLoc = CharacterOwner->GetPawnViewLocation() + (ValidatedAimDir * 40.0f);
    }

    FVector DesiredTarget = StartLoc + (ValidatedAimDir * 30.0f);
    FVector FinalSpawnLoc;
    GetSafeSpawnLocation(StartLoc, DesiredTarget, FinalSpawnLoc);

    FVector TrueLaunchDir = (TargetPoint - FinalSpawnLoc).GetSafeNormal();

    CurrentThrowableCount--;
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentThrowableCount, this);
    OnRep_ThrowableCount();

    FRotator SpawnRotation = TrueLaunchDir.Rotation();
    FTransform SpawnTransform(SpawnRotation, FinalSpawnLoc);

    AActor* SpawnedThrowable = GetWorld()->SpawnActorDeferred<AActor>(
        ThrowableClass,
        SpawnTransform,
        CharacterOwner,
        CharacterOwner,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn
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
    }

    float ReleaseAnimDuration = ThrowCooldown;
    GetWorld()->GetTimerManager().SetTimer(
        TimerHandle_ResetThrowState,
        this,
        &UThrowableComponent::ResetThrowState_Server,
        ReleaseAnimDuration,
        false
    );
}

void UThrowableComponent::ResetThrowState_Server()
{
    bIsCharging = false;
    bIsThrowingInProcess = false;

    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, bIsCharging, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, bIsThrowingInProcess, this);

    SetWeaponVisibility(false);
}

void UThrowableComponent::Client_ResetThrowState_Implementation()
{
    bIsCharging = false;
    bIsThrowingInProcess = false;

    if (CharacterOwner && CharacterOwner->GetMesh())
    {
        if (UAnimInstance* AnimInstance = CharacterOwner->GetMesh()->GetAnimInstance())
        {
            if (ThrowMontage && AnimInstance->Montage_IsPlaying(ThrowMontage))
            {
                AnimInstance->Montage_Stop(0.2f, ThrowMontage);
            }
        }
    }

    SetWeaponVisibility(false);
}

void UThrowableComponent::OnThrowMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (Montage == ThrowMontage)
    {
        bIsThrowingInProcess = false;
        SetWeaponVisibility(false);
    }
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

void UThrowableComponent::OnRep_IsCharging()
{
    SetWeaponVisibility(bIsCharging);

    if (CharacterOwner && CharacterOwner->IsLocallyControlled()) return;
    if (!ThrowMontage || !CharacterOwner || !CharacterOwner->GetMesh()) return;

    UAnimInstance* AnimInstance = CharacterOwner->GetMesh()->GetAnimInstance();
    if (!AnimInstance) return;

    if (bIsCharging)
    {
        AnimInstance->Montage_Play(ThrowMontage);
        AnimInstance->Montage_JumpToSection(HoldSectionName, ThrowMontage);
        AnimInstance->Montage_SetPlayRate(ThrowMontage, 0.0f);
    }
    else
    {
        AnimInstance->Montage_SetPlayRate(ThrowMontage, 1.0f);
        AnimInstance->Montage_JumpToSection(ReleaseSectionName, ThrowMontage);
    }
}

void UThrowableComponent::SetWeaponVisibility(bool bVisible)
{
    if (CharacterOwner && IsValid(CharacterOwner->CurrentWeapon))
    {
        CharacterOwner->CurrentWeapon->SetActorHiddenInGame(bVisible);
    }
}

void UThrowableComponent::RefillThrowables(int32 Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    CurrentThrowableCount = FMath::Clamp(CurrentThrowableCount + Amount, 0, MaxThrowableCount);
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentThrowableCount, this);

    OnRep_ThrowableCount();
}

void UThrowableComponent::OnRep_ThrowableCount()
{
    OnThrowableCountChanged.Broadcast(CurrentThrowableCount);
}