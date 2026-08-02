#include "HamaPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Hama.h"
#include "HamaAbilityComponent.h"

AHamaPlayerState::AHamaPlayerState()
{
    SetNetUpdateFrequency(10.f);
    SetMinNetUpdateFrequency(2.f);

    Points = 500;
    Kills = 0;
}

void AHamaPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams SharedParams;
    SharedParams.bIsPushBased = true;

    DOREPLIFETIME_WITH_PARAMS_FAST(AHamaPlayerState, Points, SharedParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(AHamaPlayerState, Kills, SharedParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(AHamaPlayerState, AssignedRole, SharedParams);
}

void AHamaPlayerState::SetAssignedRole(EHamaAbilityType NewRole)
{
    if (HasAuthority())
    {
        AssignedRole = NewRole;
        MARK_PROPERTY_DIRTY_FROM_NAME(AHamaPlayerState, AssignedRole, this);

        if (AHama* HamaChar = Cast<AHama>(GetPawn()))
        {
            HamaChar->ApplyRoleVisuals(AssignedRole);
            if (UHamaAbilityComponent* AbilityComp = HamaChar->FindComponentByClass<UHamaAbilityComponent>())
            {
                AbilityComp->SetAssignedAbility(AssignedRole);
            }
        }
    }
}

void AHamaPlayerState::OnRep_AssignedRole()
{
    if (AHama* HamaChar = Cast<AHama>(GetPawn()))
    {
        HamaChar->ApplyRoleVisuals(AssignedRole);

        if (UHamaAbilityComponent* AbilityComp = HamaChar->FindComponentByClass<UHamaAbilityComponent>())
        {
            AbilityComp->SetAssignedAbility(AssignedRole);
        }
    }
}

void AHamaPlayerState::AddPoints(int32 Amount)
{
    if (HasAuthority() && Amount > 0)
    {
        Points += Amount;
        MARK_PROPERTY_DIRTY_FROM_NAME(AHamaPlayerState, Points, this);
        Client_OnPointGained(Points);
        OnRep_Points();
    }
}

void AHamaPlayerState::RemovePoints(int32 Amount)
{
    if (HasAuthority() && Amount > 0)
    {
        Points = FMath::Max(0, Points - Amount);
        MARK_PROPERTY_DIRTY_FROM_NAME(AHamaPlayerState, Points, this);
        Client_OnPointGained(Points);
        OnRep_Points();
    }
}

void AHamaPlayerState::Client_OnPointGained_Implementation(int32 NewPoints)
{
    OnPointsChanged.ExecuteIfBound(NewPoints);
}

void AHamaPlayerState::AddKills(int32 Amount)
{
    if (HasAuthority() && Amount > 0)
    {
        Kills += Amount;
        MARK_PROPERTY_DIRTY_FROM_NAME(AHamaPlayerState, Kills, this);
        Client_OnKillGained(Kills);
        OnRep_Kills();
    }
}

void AHamaPlayerState::Client_OnKillGained_Implementation(int32 NewKill)
{
    OnKillsChanged.ExecuteIfBound(NewKill);
}

void AHamaPlayerState::OnRep_Points()
{
    OnPointsChanged.ExecuteIfBound(Points);
}

void AHamaPlayerState::OnRep_Kills()
{
    OnKillsChanged.ExecuteIfBound(Kills);
}