#include "HamaPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"

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

    FDoRepLifetimeParams SharedParams;
    SharedParams.bIsPushBased = true;

    DOREPLIFETIME_WITH_PARAMS_FAST(AHamaPlayerState, Points, SharedParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(AHamaPlayerState, Kills, SharedParams);
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

        MARK_PROPERTY_DIRTY_FROM_NAME(AHamaPlayerState, Points, this);

        OnRep_Points();
    }
}

void AHamaPlayerState::RemovePoints(int32 Amount)
{
    if (HasAuthority())
    {
        Points = FMath::Max(0, Points - Amount);

        MARK_PROPERTY_DIRTY_FROM_NAME(AHamaPlayerState, Points, this);

        OnRep_Points();
    }
}

void AHamaPlayerState::AddKills(int32 Amount)
{
    if (HasAuthority())
    {
        Kills += Amount;

        MARK_PROPERTY_DIRTY_FROM_NAME(AHamaPlayerState, Kills, this);

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