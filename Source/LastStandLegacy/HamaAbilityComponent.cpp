#include "HamaAbilityComponent.h"
#include "Net/UnrealNetwork.h"
#include "Hama.h"
#include "LastStandLegacyGameState.h"
#include "CollisionQueryParams.h"
#include "HamaPlayerState.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"

UHamaAbilityComponent::UHamaAbilityComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UHamaAbilityComponent::BeginPlay()
{
    Super::BeginPlay();

    GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
        {
            if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
            {
                if (AHamaPlayerState* PS = OwnerPawn->GetPlayerState<AHamaPlayerState>())
                {
                    SetAssignedAbility(PS->GetAssignedRole());
                }
            }
        });
}

void UHamaAbilityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UHamaAbilityComponent, CurrentPower, COND_OwnerOnly);
    DOREPLIFETIME(UHamaAbilityComponent, bIsGhost);
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

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Magenta, FString::Printf(TEXT("Current Power is: %f"), CurrentPower));

    // نوێکردنەوەی شاشە بۆ ئەو یاریزانەی کە هۆستە
    if (GetOwner()->GetLocalRole() == ROLE_Authority && GetNetMode() != NM_DedicatedServer)
    {
        OnRep_CurrentPower();
    }
}

void UHamaAbilityComponent::ResetPower()
{
    CurrentPower = 0.f;
    if (GetOwner()->GetLocalRole() == ROLE_Authority && GetNetMode() != NM_DedicatedServer)
    {
        OnRep_CurrentPower();
    }
}

bool UHamaAbilityComponent::IsPowerFull() const
{
    return CurrentPower >= MaxPower;
}

void UHamaAbilityComponent::FullPower()
{
    CurrentPower = MaxPower;
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Magenta, FString::Printf(TEXT("Current Power is: %f"), CurrentPower));
    if (GetOwner()->GetLocalRole() == ROLE_Authority && GetNetMode() != NM_DedicatedServer)
    {
        OnRep_CurrentPower();
    }
}

void UHamaAbilityComponent::OnRep_CurrentPower()
{
    OnPowerChanged.Broadcast(CurrentPower);
}

void UHamaAbilityComponent::Server_ActivateAbility_Implementation()
{
    if (CurrentPower < MaxPower || CurrentAssignedAbility == EHamaAbilityType::None) return;
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

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
    case EHamaAbilityType::Blitz:
        ActivateBlitz();
        break;
    default:
        UE_LOG(LogTemp, Warning, TEXT("Player tried to activate ability but has NONE assigned!"));
        break;
    }
}

void UHamaAbilityComponent::ActivateBulletStorm()
{
    UWorld* World = GetWorld();
    if (!World) return;

    if (ALastStandLegacyGameState* GS = World->GetGameState<ALastStandLegacyGameState>())
    {
        GS->StartGlobalBulletStorm(AbilityDuration);
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("BulletStorm Activated on Server!"));
        ResetPower();
    }
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("BulletStorm Failed!"));
}

void UHamaAbilityComponent::ActivateMedicalSupport()
{
    UWorld* World = GetWorld();
    AActor* Owner = GetOwner();
    if (!World || !Owner) return;

    FVector CenterLocation = Owner->GetActorLocation();
    TArray<FOverlapResult> OverlapResults; // گۆڕا بۆ OverlapResult
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(SphereRadius);

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Owner); // خۆت فەرامۆش دەکەیت بۆ ئەوەی خۆت نەگریت

    // بەکارهێنانی Overlap لەبری Sweep
    bool bHit = World->OverlapMultiByChannel(OverlapResults, CenterLocation, FQuat::Identity, ECC_Pawn, SphereShape, Params);
    bool bSuccessfullyRevivedSomeone = false;

    if (bHit)
    {
        for (const FOverlapResult& Overlap : OverlapResults)
        {
            // وەرگرتنی ئەکتەرەکە لە Overlapـەوە
            if (AHama* Hama = Cast<AHama>(Overlap.GetActor()))
            {
                if (UHamaComponent* HamaComponent = Hama->FindComponentByClass<UHamaComponent>())
                {
                    if (HamaComponent->IsDowned())
                    {
                        HamaComponent->Revive();
                        bSuccessfullyRevivedSomeone = true;
                    }
                }
            }
        }
    }

    if (bSuccessfullyRevivedSomeone)
    {
        ResetPower();
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Medical Support Successfully Revived Player(s)!"));
    }
    else
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, TEXT("Medical Support Failed: No downed players nearby. Power kept."));
    }
}

void UHamaAbilityComponent::ActivateGhostMode()
{
    if (bIsGhost) return;

    bIsGhost = true;
    ResetPower();

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Purple, TEXT("Ghost Mode Activated on Server!"));

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            GhostTimerHandle,
            this,
            &UHamaAbilityComponent::DeactivateGhostMode,
            AbilityDuration,
            false
        );
    }
    OnRep_IsGhost();
}

void UHamaAbilityComponent::DeactivateGhostMode()
{
    bIsGhost = false;

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Ghost Mode Deactivated!"));

    OnRep_IsGhost();
}

void UHamaAbilityComponent::ActivateBlitz()
{
    UWorld* World = GetWorld();
    if (!World) return;

    if (ALastStandLegacyGameState* GS = World->GetGameState<ALastStandLegacyGameState>())
    {
        GS->StartTeamAdrenaline(BlitzAbilityDuration);
        ResetPower();
    }

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, TEXT("Blitz Activated on Server!"));
}

void UHamaAbilityComponent::OnRep_IsGhost()
{
    if (bIsGhost)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Purple, TEXT("Visuals: Player became a Ghost"));
    }
    else
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("Visuals: Player returned to Normal"));
    }
}

void UHamaAbilityComponent::StopAllAbilities()
{
    UWorld* World = GetWorld();
    if (!World) return;

    World->GetTimerManager().ClearAllTimersForObject(this);

    if (bIsGhost)
    {
        bIsGhost = false;
        if (GetOwner() && GetOwner()->HasAuthority())
        {
            OnRep_IsGhost();
        }
    }
}