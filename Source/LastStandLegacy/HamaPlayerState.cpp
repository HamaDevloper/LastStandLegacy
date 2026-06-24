#include "HamaPlayerState.h"
#include "Net/UnrealNetwork.h"

AHamaPlayerState::AHamaPlayerState()
{
    SetNetUpdateFrequency(2.f);
    SetMinNetUpdateFrequency(1.f);

    Points = 500;
    Kills = 0;
}

void AHamaPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AHamaPlayerState, Points);
    DOREPLIFETIME(AHamaPlayerState, Kills);
    DOREPLIFETIME_CONDITION(AHamaPlayerState, AssignedRole, COND_InitialOnly);
}

void AHamaPlayerState::SetAssignedRole(EHamaAbilityType NewRole)
{
    if (HasAuthority())
    {
        AssignedRole = NewRole;
        ForceNetUpdate();
    }
}

void AHamaPlayerState::AddPoints(int32 Amount)
{
    if (HasAuthority())
    {
        Points += Amount;
        ForceNetUpdate();
        OnRep_Points();
    }
}

void AHamaPlayerState::AddKills(int32 Amount)
{
    if (HasAuthority())
    {
        Kills += Amount;
        ForceNetUpdate();
        OnRep_Kills();
    }
}

void AHamaPlayerState::OnRep_Points()
{
    OnPointsChanged.ExecuteIfBound(Points);
}

void AHamaPlayerState::OnRep_Kills()
{
    OnKillsChanged.ExecuteIfBound(Kills);
}