#include "HamaPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Hama.h"
#include "HamaAbilityComponent.h"
#include "GobbleGumComponent.h"

AHamaPlayerState::AHamaPlayerState()
{
    SetNetUpdateFrequency(10.f);
    SetMinNetUpdateFrequency(2.f);

    Kills = 0;
}

void AHamaPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams SharedParams;
    SharedParams.bIsPushBased = true;

    SharedParams.Condition = COND_None;
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

void AHamaPlayerState::SetPoints(int32 NewPoints)
{
    if (HasAuthority())
    {
        Points = FMath::Clamp(NewPoints, 0, MaxPointToEarn);
        MARK_PROPERTY_DIRTY_FROM_NAME(AHamaPlayerState, Points, this);

        OnRep_Points();
    }
}

void AHamaPlayerState::AddPoints(int32 Amount, bool bAffectedByMultiplier)
{
    if (HasAuthority() && Amount > 0)
    {
        int32 FinalAmount = Amount;

        if (bAffectedByMultiplier)
        {
            if (const AHama* PlayerChar = GetPawn<AHama>())
            {
                if (const UGobbleGumComponent* GumComp = PlayerChar->GetGobbleGumComponent())
                {
                    FinalAmount = FMath::RoundToInt(static_cast<float>(Amount) * GumComp->GetPointsMultiplier());
                }
            }
        }

        Points = FMath::Clamp(Points + FinalAmount, 0, MaxPointToEarn);
        MARK_PROPERTY_DIRTY_FROM_NAME(AHamaPlayerState, Points, this);

        OnRep_Points();
    }
}

void AHamaPlayerState::RemovePoints(int32 Amount)
{
    if (HasAuthority() && Amount > 0)
    {
        Points = FMath::Max(0, Points - Amount);
        MARK_PROPERTY_DIRTY_FROM_NAME(AHamaPlayerState, Points, this);
        
        OnRep_Points();
    }
}

void AHamaPlayerState::AddKills(int32 Amount)
{
    if (HasAuthority() && Amount > 0)
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

void AHamaPlayerState::SetLegacyKeeperArmed(bool bArmed)
{
    if (!HasAuthority()) return;
    bIsLegacyKeeperArmed = bArmed;
}

void AHamaPlayerState::SaveLoadoutForLegacyKeeper(const TArray<FLegacySavedWeapon>& WeaponsToSave)
{
    if (!HasAuthority() || !bIsLegacyKeeperArmed || WeaponsToSave.Num() == 0) return;

    SavedLegacyLoadout = WeaponsToSave;
    bHasPendingLegacyLoadout = true;
    bIsLegacyKeeperArmed = false;
}

TArray<FLegacySavedWeapon> AHamaPlayerState::ConsumePendingLegacyLoadout()
{
    TArray<FLegacySavedWeapon> Result = MoveTemp(SavedLegacyLoadout);
    SavedLegacyLoadout.Empty();
    bHasPendingLegacyLoadout = false;
    return Result;
}