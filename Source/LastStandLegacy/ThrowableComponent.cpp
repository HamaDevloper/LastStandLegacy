#include "ThrowableComponent.h"
#include "Hama.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "GameFramework/Character.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

UThrowableComponent::UThrowableComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
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
}

void UThrowableComponent::RequestThrow()
{
    if (!CharacterOwner || CurrentThrowableCount <= 0) return;

    if (ThrowMontage && CharacterOwner->GetMesh())
    {
        UAnimInstance* AnimInstance = CharacterOwner->GetMesh()->GetAnimInstance();
        if (AnimInstance && AnimInstance->Montage_IsPlaying(ThrowMontage))
        {
            return;
        }

        if (AnimInstance)
        {
            AnimInstance->Montage_Play(ThrowMontage);
        }
    }

    FVector LaunchDir = CharacterOwner->GetControlRotation().Vector();
    Server_Throw(LaunchDir);
}

void UThrowableComponent::Server_Throw_Implementation(FVector_NetQuantize10 LaunchDirection)
{
    if (!CharacterOwner || !ThrowableClass || CurrentThrowableCount <= 0) return;

    FVector ServerControlDir = CharacterOwner->GetControlRotation().Vector();
    float AngleDot = FVector::DotProduct(ServerControlDir, LaunchDirection);

    if (AngleDot < 0.7f)
    {
        LaunchDirection = ServerControlDir; 
    }

    const float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastThrowTime < ThrowCooldown)
    {
        return;
    }

    if (LaunchDirection.IsNearlyZero())
    {
        return;
    }

    LastThrowTime = CurrentTime;

    FVector SafeDir = LaunchDirection.GetSafeNormal();
    FVector HandLoc = CharacterOwner->GetMesh()->GetSocketLocation(HandSocketName);
    FVector ViewLoc = CharacterOwner->GetPawnViewLocation();

    FVector FinalSpawnLoc;
    GetSafeSpawnLocation(ViewLoc, HandLoc, FinalSpawnLoc);

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
            ProjComp->Velocity = SafeDir * ThrowImpulseStrength;
        }

        UGameplayStatics::FinishSpawningActor(SpawnedThrowable, SpawnTransform);
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

void UThrowableComponent::OnRep_ThrowableCount()
{
    OnThrowableCountChanged.Broadcast(CurrentThrowableCount);
}