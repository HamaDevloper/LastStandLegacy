// Fill out your copyright notice in the Description page of Project Settings.

#include "HamaAbilityComponent.h"
#include "Net/UnrealNetwork.h"
#include "Hama.h"
#include "LastStandLegacyGameState.h"

UHamaAbilityComponent::UHamaAbilityComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UHamaAbilityComponent::BeginPlay()
{
    Super::BeginPlay();
}

// ڕێکخستنی نێتۆرک بۆ ناردنی داتای تواناکە تەنها بۆ خودی ئەو یاریزانە
void UHamaAbilityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UHamaAbilityComponent, CurrentAssignedAbility);
    DOREPLIFETIME_CONDITION(UHamaAbilityComponent, CurrentPower, COND_OwnerOnly);
}

void UHamaAbilityComponent::SetAssignedAbility(EHamaAbilityType NewAbility)
{
    CurrentAssignedAbility = NewAbility;

    if (AHama* HamaOwner = Cast<AHama>(GetOwner()))
    {
        HamaOwner->OnRoleAssigned_BP(CurrentAssignedAbility);
    }
}

void UHamaAbilityComponent::AddPower(float Amount)
{
    if (CurrentPower >= MaxPower) return;
    if (!GetOwner()->HasAuthority() || CurrentAssignedAbility == EHamaAbilityType::None) return;

    CurrentPower = FMath::Clamp(CurrentPower + Amount, 0.f, MaxPower);

    if (GetOwner()->GetLocalRole() == ROLE_Authority && GetNetMode() != NM_DedicatedServer)
    {
        OnRep_CurrentPower();
    }
}

void UHamaAbilityComponent::OnRep_CurrentAssignedAbility()
{
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow,
            TEXT("Your assigned ability has replicated from Server!"));
    }
}

void UHamaAbilityComponent::OnRep_CurrentPower()
{
    if (AHama* HamaOwner = Cast<AHama>(GetOwner()))
    {
        HamaOwner->OnRoleAssigned_BP(CurrentAssignedAbility);
    }
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
}

void UHamaAbilityComponent::ActivateMedicalSupport()
{
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Medical Support Activated on Server!"));
}

void UHamaAbilityComponent::ActivateGhostMode()
{
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Purple, TEXT("Ghost Mode Activated on Server!"));
}

void UHamaAbilityComponent::ActivateDecoy()
{
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, TEXT("Decoy Activated on Server!"));
}