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
}

void UThrowableComponent::StartThrowCharge()
{
    if (!CharacterOwner)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[StartThrowCharge] RETURN: CharacterOwner is NULL!"));
        return;
    }

    if (CurrentThrowableCount <= 0)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString::Printf(TEXT("[StartThrowCharge] RETURN: No Ammo! Count: %d"), CurrentThrowableCount));
        return;
    }

    if (bIsCharging)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[StartThrowCharge] RETURN: Already Charging!"));
        return;
    }

    bIsCharging = true;

    if (ThrowMontage && CharacterOwner->GetMesh())
    {
        if (UAnimInstance* AnimInstance = CharacterOwner->GetMesh()->GetAnimInstance())
        {
            AnimInstance->Montage_Play(ThrowMontage);
            AnimInstance->Montage_JumpToSection(HoldSectionName, ThrowMontage);
            AnimInstance->Montage_SetPlayRate(ThrowMontage, 0.0f);
        }
        else
        {
            if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[StartThrowCharge] ERROR: AnimInstance is NULL!"));
        }
    }
    else
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[StartThrowCharge] ERROR: ThrowMontage or CharacterMesh Missing!"));
    }

    if (!GetOwner()->HasAuthority())
    {
        Server_StartThrowCharge();
    }
}

void UThrowableComponent::Server_StartThrowCharge_Implementation()
{
    if (!CharacterOwner || CurrentThrowableCount <= 0 || bIsCharging)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[Server_StartThrowCharge] RETURN: Validation Failed on Server!"));
        return;
    }

    bIsCharging = true;
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, bIsCharging, this);

    if (GetNetMode() == NM_ListenServer && !CharacterOwner->IsLocallyControlled())
    {
        OnRep_IsCharging();
    }
}

void UThrowableComponent::ReleaseThrow()
{
    if (!CharacterOwner || !bIsCharging) return;

    // 🛑 1. چاککردنی ئەنیمەیشن: دەستبەجێ خێرایی دەکەینەوە 1.0f و پاشان Jump دەبەستین
    if (ThrowMontage && CharacterOwner->GetMesh())
    {
        if (UAnimInstance* AnimInstance = CharacterOwner->GetMesh()->GetAnimInstance())
        {
            AnimInstance->Montage_SetPlayRate(ThrowMontage, 1.0f);
            AnimInstance->Montage_JumpToSection(ReleaseSectionName, ThrowMontage);
        }
    }

    FVector LaunchDir = CharacterOwner->GetControlRotation().Vector();

    if (!GetOwner()->HasAuthority())
    {
        bIsCharging = false;
    }

    Server_ReleaseThrow(LaunchDir);
}

void UThrowableComponent::Server_ReleaseThrow_Implementation(FVector_NetQuantizeNormal LaunchDirection)
{
    if (!CharacterOwner || !ThrowableClass || CurrentThrowableCount <= 0 || !bIsCharging) return;

    const float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastThrowTime < ThrowCooldown) return;

    bIsCharging = false;
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, bIsCharging, this);

    if (GetNetMode() == NM_ListenServer && !CharacterOwner->IsLocallyControlled())
    {
        OnRep_IsCharging();
    }

    LastThrowTime = CurrentTime;

    FVector SafeDir = LaunchDirection.GetSafeNormal();

    // 🛑 1. دوورخستنەوەی خاڵی Spawn بە بڕی 60 یەکە بۆ پێشەوە لە چاوی کاراکتەرەکە (ڕێگری لە Clipping)
    FVector ViewLoc = CharacterOwner->GetPawnViewLocation();
    FVector FinalSpawnLoc = ViewLoc + (SafeDir * 60.0f);

    CurrentThrowableCount--;
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentThrowableCount, this);
    OnRep_ThrowableCount();

    FTransform SpawnTransform(CharacterOwner->GetControlRotation(), FinalSpawnLoc);

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
}

void UThrowableComponent::OnRep_IsCharging()
{
    if (CharacterOwner && CharacterOwner->IsLocallyControlled()) return;

    if (!ThrowMontage || !CharacterOwner || !CharacterOwner->GetMesh())
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[OnRep_IsCharging] RETURN: Missing Mesh or Montage on Client!"));
        return;
    }

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

void UThrowableComponent::RefillThrowables(int32 Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[RefillThrowables] RETURN: No Authority!"));
        return;
    }

    CurrentThrowableCount = FMath::Clamp(CurrentThrowableCount + Amount, 0, MaxThrowableCount);
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentThrowableCount, this);

    OnRep_ThrowableCount();
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, FString::Printf(TEXT("[RefillThrowables] SUCCESS: New Count: %d"), CurrentThrowableCount));
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