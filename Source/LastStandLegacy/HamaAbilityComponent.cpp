#include "HamaAbilityComponent.h"
#include "Net/UnrealNetwork.h"
#include "Hama.h"
#include "LastStandLegacyGameState.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"

UHamaAbilityComponent::UHamaAbilityComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UHamaAbilityComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UHamaAbilityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UHamaAbilityComponent, CurrentPower, COND_OwnerOnly);
}

void UHamaAbilityComponent::SetAssignedAbility(EHamaAbilityType NewAbility)
{
    CurrentAssignedAbility = NewAbility;
}

void UHamaAbilityComponent::AddPower(float Amount)
{
    if (CurrentPower >= MaxPower) return;
    if (!GetOwner()->HasAuthority() || CurrentAssignedAbility == EHamaAbilityType::None) return;

    CurrentPower = FMath::Clamp(CurrentPower + Amount, 0.f, MaxPower);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Magenta, FString::Printf(TEXT("Current Power is: %f"), CurrentPower));
    }
    if (GetOwner()->GetLocalRole() == ROLE_Authority && GetNetMode() != NM_DedicatedServer)
    {
        OnRep_CurrentPower();
    }
}

bool UHamaAbilityComponent::IsPowerFull() const
{
    return CurrentPower >= MaxPower;
}

void UHamaAbilityComponent::OnRep_CurrentPower()
{
    OnPowerChanged.Broadcast(CurrentPower);
}

void UHamaAbilityComponent::Server_ActivateAbility_Implementation()
{
    if (CurrentPower < MaxPower) return;

    switch (CurrentAssignedAbility)
    {
    case EHamaAbilityType::BulletStorm:
        ActivateBulletStorm();
        break;
    case EHamaAbilityType::MedicalSupport:
        ActivateMedicalSupport();
        break;
    case EHamaAbilityType::GhostMode:
        ActivateGhostMode();
        break;
    case EHamaAbilityType::Decoy:
        ActivateDecoy();
        break;
    default:
        UE_LOG(LogTemp, Warning, TEXT("Player tried to activate ability but has NONE assigned!"));
        break;
    }
    CurrentPower = 0.f;
}

void UHamaAbilityComponent::ActivateBulletStorm()
{
    ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>();
    if (GS)
    {
        GS->StartGlobalBulletStorm(BulletStormDuration);
    }
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("BulletStorm Activated on Server!"));
}

void UHamaAbilityComponent::ActivateMedicalSupport()
{
    AActor* Owner = GetOwner();
    if (!Owner) return;

    UWorld* World = GetWorld();
    if (!World) return;

    if (!Owner->HasAuthority()) return;

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Medical Support Activated on Server!"));

    FVector StartLocation = Owner->GetActorLocation();
    FVector EndLocation = StartLocation;
    float SphereRadius = 500.f;

    TArray<FHitResult> HitResults;
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(SphereRadius);

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Owner);

    bool bHit = World->SweepMultiByChannel(HitResults, StartLocation, EndLocation, FQuat::Identity, ECC_Pawn, SphereShape, Params);

    if (bHit)
    {
        for (const FHitResult& Hit : HitResults)
        {
            if (AHama* Hama = Cast<AHama>(Hit.GetActor()))
            {
                if (UHamaComponent* HamaComponent = Hama->FindComponentByClass<UHamaComponent>())
                {
                    if (HamaComponent->IsDowned())
                    {
                        HamaComponent->Revive();
                    }
                }
            }
        }
    }
}

void UHamaAbilityComponent::ActivateGhostMode()
{
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Purple, TEXT("Ghost Mode Activated on Server!"));
}

void UHamaAbilityComponent::ActivateDecoy()
{
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, TEXT("Decoy Activated on Server!"));
}