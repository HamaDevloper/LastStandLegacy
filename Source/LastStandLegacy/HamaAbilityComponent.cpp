// Fill out your copyright notice in the Description page of Project Settings.

#include "HamaAbilityComponent.h"
#include "Net/UnrealNetwork.h"
#include "Hama.h"

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
    DOREPLIFETIME_CONDITION(UHamaAbilityComponent, CurrentAssignedAbility, COND_OwnerOnly);
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
    OnPowerChanged.Broadcast(CurrentPower);
}

void UHamaAbilityComponent::Server_ActivateAbility_Implementation()
{
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
    currentPower = 0.f;
}

void UHamaAbilityComponent::ActivateBulletStorm()
{
    GetWorld()->GetTimerManager().SetTimer(BulletStormTimerHandle, this, &UHamaAbilityComponent::DeactivateBulletStorm, BulletStormDuration, false);
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

void UHamaAbilityComponent::DeactivateBulletStorm()
{
    GetWorld()->GetTimerManager().ClearTimer(BulletStormTimerHandle);
}
