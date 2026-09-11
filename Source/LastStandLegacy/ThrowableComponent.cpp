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
    if (!CharacterOwner || CurrentThrowableCount <= 0 || bIsCharging || bIsThrowingInProcess) return;

    const float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastThrowTime < ThrowCooldown) return;

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

    if (!CharacterOwner || CurrentThrowableCount <= 0 || (CurrentTime - LastThrowTime < ThrowCooldown))
    {
        Client_ResetThrowState();
        return;
    }

    bIsCharging = true;
    bIsThrowingInProcess = true;
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, bIsCharging, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, bIsThrowingInProcess, this);

    if (GetNetMode() == NM_ListenServer && !CharacterOwner->IsLocallyControlled())
    {
        OnRep_IsCharging();
    }
}

void UThrowableComponent::ReleaseThrow()
{
    if (!CharacterOwner || !bIsCharging) return;

    if (ThrowMontage && CharacterOwner->GetMesh())
    {
        if (UAnimInstance* AnimInstance = CharacterOwner->GetMesh()->GetAnimInstance())
        {
            AnimInstance->Montage_SetPlayRate(ThrowMontage, 1.0f);
            AnimInstance->Montage_JumpToSection(ReleaseSectionName, ThrowMontage);
        }
    }

    // 🛠️ تەنها ئاڕاستەی بەرەوپێشچوونی کامێرا (Normalized Vector) بنێرە
    FVector AimDir = GetCameraAimDirection();

    if (!GetOwner()->HasAuthority())
    {
        bIsCharging = false;
    }

    Server_ReleaseThrow(AimDir);
}

void UThrowableComponent::Server_ReleaseThrow_Implementation(FVector_NetQuantizeNormal LaunchDirection)
{
    const float CurrentTime = GetWorld()->GetTimeSeconds();

    if (!CharacterOwner || !ThrowableClass || CurrentThrowableCount <= 0 || !bIsCharging || (CurrentTime - LastThrowTime < ThrowCooldown))
    {
        Client_ResetThrowState();
        return;
    }

    LastThrowTime = CurrentTime;
    bIsCharging = false;
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, bIsCharging, this);

    // 🛠️ 1. پشکنینی ئاڕاستەی تەواو لە سەرڤەر بەپێی Vector نێردراوەکە
    FVector ValidatedAimDir = LaunchDirection.GetSafeNormal();
    FVector TargetPoint = GetCrosshairTargetPoint(ValidatedAimDir);

    // 🛠️ 2. دۆزینەوەی شوێنی سەلامەت لەبەردەم کامێرا/Pawn
    FVector StartLoc = CharacterOwner->GetPawnViewLocation();
    FVector DesiredTarget = StartLoc + (ValidatedAimDir * 50.0f);
    FVector FinalSpawnLoc;
    GetSafeSpawnLocation(StartLoc, DesiredTarget, FinalSpawnLoc);

    // 🛠️ 3. ئاڕاستەی دروستی فڕێدان بەبێ Double Rotation
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

    bIsThrowingInProcess = false;
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
    SetWeaponVisibility(!bIsCharging);

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