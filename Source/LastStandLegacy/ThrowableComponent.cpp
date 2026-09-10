#include "ThrowableComponent.h"
#include "Hama.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "GameFramework/Character.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

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
        bIsCharging = false;
        bIsThrowingInProcess = false;
        MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, bIsCharging, this);
        MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, bIsThrowingInProcess, this);

        SetWeaponVisibility(true);
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


FVector UThrowableComponent::GetCrosshairAimDirection() const
{
    if (!CharacterOwner) return FVector::ForwardVector;

    return CharacterOwner->GetControlRotation().Vector();
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

    FVector LaunchDir = GetCrosshairAimDirection();

    if (!GetOwner()->HasAuthority())
    {
        bIsCharging = false;
    }

    Server_ReleaseThrow(LaunchDir);
}

// 2. نوێکردنەوەی شوێنی دروستبوونی بۆمبەکە لە سێرڤەر
void UThrowableComponent::Server_ReleaseThrow_Implementation(FVector_NetQuantizeNormal LaunchDirection)
{
    const float CurrentTime = GetWorld()->GetTimeSeconds();

    if (!CharacterOwner || !ThrowableClass || CurrentThrowableCount <= 0 || !bIsCharging || (CurrentTime - LastThrowTime < ThrowCooldown))
    {
        bIsCharging = false;
        bIsThrowingInProcess = false;
        MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, bIsCharging, this);
        MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, bIsThrowingInProcess, this);
        SetWeaponVisibility(false);
        return;
    }

    LastThrowTime = CurrentTime;

    bIsCharging = false;
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, bIsCharging, this);

    FVector SafeDir = LaunchDirection.GetSafeNormal();

    // 🛑 چارەسەری بنەڕەتی: سپاونکردن ڕێک لە ناوەڕاستی شاشەکەوە (Camera) بە دووری 80 یەکە پێشەوە.
    // هەرگیز سۆکێتی دەست بەکارمەهێنە لێرەدا، چونکە کێشەی Desync و لاربوونەوە دروست دەکات.
    FVector SpawnLoc = CharacterOwner->GetPawnViewLocation() + (SafeDir * 80.0f);

    CurrentThrowableCount--;
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentThrowableCount, this);
    OnRep_ThrowableCount();

    FRotator SpawnRotation = SafeDir.Rotation();
    FTransform SpawnTransform(SpawnRotation, SpawnLoc);

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
            ProjComp->MaxSpeed = FMath::Max(ProjComp->MaxSpeed, ThrowImpulseStrength);
            ProjComp->Velocity = SafeDir * ThrowImpulseStrength;
        }

        UGameplayStatics::FinishSpawningActor(SpawnedThrowable, SpawnTransform);
    }

    bIsThrowingInProcess = false;
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, bIsThrowingInProcess, this);

    SetWeaponVisibility(false);
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

void UThrowableComponent::OnRep_ThrowableCount()
{
    OnThrowableCountChanged.Broadcast(CurrentThrowableCount);
}