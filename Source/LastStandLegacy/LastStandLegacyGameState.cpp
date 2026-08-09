#include "LastStandLegacyGameState.h"
#include "Hama.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"

ALastStandLegacyGameState::ALastStandLegacyGameState()
{
    SetNetUpdateFrequency(2.f);
    SetMinNetUpdateFrequency(1.f);
}

void ALastStandLegacyGameState::Multicast_AnnouncePowerUp_Implementation(EPowerUpType PowerUpType)
{
    if(OnPowerUpAnnouncedDelegate.IsBound())
    {
        OnPowerUpAnnouncedDelegate.Broadcast(PowerUpType);
    }
}

void ALastStandLegacyGameState::BeginPlay()
{
    Super::BeginPlay();
}

void ALastStandLegacyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Params;
    Params.bIsPushBased = true;

    DOREPLIFETIME_WITH_PARAMS_FAST(ALastStandLegacyGameState, CurrentRound, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(ALastStandLegacyGameState, bIsGlobalBulletStormActive, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(ALastStandLegacyGameState, bIsDoublePointsActive, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(ALastStandLegacyGameState, bHasInstaKill, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(ALastStandLegacyGameState, bIsAdrenalineActive, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(ALastStandLegacyGameState, bIsPowerOn, Params);
    DOREPLIFETIME_CONDITION(ALastStandLegacyGameState, bIsSoloMatch, COND_InitialOnly);
}

void ALastStandLegacyGameState::RegisterTarget(APawn* NewTarget)
{
    if (HasAuthority() && NewTarget && !ValidTargets.Contains(NewTarget))
    {
        ValidTargets.Add(NewTarget);
    }
}

void ALastStandLegacyGameState::UnregisterTarget(APawn* TargetToRemove)
{
    if (HasAuthority() && TargetToRemove)
    {
        ValidTargets.Remove(TargetToRemove);
    }
}

void ALastStandLegacyGameState::SetCurrentRound(int32 NewRound)
{
    if (!HasAuthority()) return;

    CurrentRound = NewRound;
    MARK_PROPERTY_DIRTY_FROM_NAME(ALastStandLegacyGameState, CurrentRound, this);

    OnRoundChangedDelegate.Broadcast(CurrentRound);
}

void ALastStandLegacyGameState::OnRep_CurrentRound()
{
    OnRoundChangedDelegate.Broadcast(CurrentRound);
}

void ALastStandLegacyGameState::SetPowerState(bool bNewPowerState)
{
    if (!HasAuthority() || bIsPowerOn == bNewPowerState) return;

    bIsPowerOn = bNewPowerState;

    FlushNetDormancy();
    ForceNetUpdate();

    MARK_PROPERTY_DIRTY_FROM_NAME(ALastStandLegacyGameState, bIsPowerOn, this);

    if (OnPowerStateChangedDelegate.IsBound())
    {
        OnPowerStateChangedDelegate.Broadcast(bIsPowerOn);
    }

    if (!IsRunningDedicatedServer())
    {
        OnRep_PowerOn();
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("SERVER: Power State changed to: %s"), bIsPowerOn ? TEXT("ON") : TEXT("OFF")));
    }
}

void ALastStandLegacyGameState::OnRep_PowerOn()
{
    if (OnPowerStateChangedDelegate.IsBound())
    {
        OnPowerStateChangedDelegate.Broadcast(bIsPowerOn);
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, FString::Printf(TEXT("CLIENT: Power State Replicated: %s"), bIsPowerOn ? TEXT("ON") : TEXT("OFF")));
    }
}

void ALastStandLegacyGameState::StartTeamAdrenaline(float Duration)
{
    if (HasAuthority())
    {
        bIsAdrenalineActive = true;
        MARK_PROPERTY_DIRTY_FROM_NAME(ALastStandLegacyGameState, bIsAdrenalineActive, this);

        OnRep_Adrenaline();
        GetWorldTimerManager().SetTimer(AdrenalineTimerHandle, this, &ALastStandLegacyGameState::EndTeamAdrenaline, Duration, false);
    }
}

void ALastStandLegacyGameState::EndTeamAdrenaline()
{
    if (HasAuthority())
    {
        bIsAdrenalineActive = false;
        
        MARK_PROPERTY_DIRTY_FROM_NAME(ALastStandLegacyGameState, bIsAdrenalineActive, this);

        OnRep_Adrenaline();
    }
}

void ALastStandLegacyGameState::StartGlobalBulletStorm(float Duration)
{
    if (HasAuthority())
    {
        bIsGlobalBulletStormActive = true;

        MARK_PROPERTY_DIRTY_FROM_NAME(ALastStandLegacyGameState, bIsGlobalBulletStormActive, this);
        OnRep_GlobalBulletStorm();

        GetWorldTimerManager().SetTimer(BulletStormTimer, this, &ALastStandLegacyGameState::EndGlobalBulletStorm, Duration, false);
    }
}

void ALastStandLegacyGameState::EndGlobalBulletStorm()
{
    if (HasAuthority())
    {
        bIsGlobalBulletStormActive = false;

        MARK_PROPERTY_DIRTY_FROM_NAME(ALastStandLegacyGameState, bIsGlobalBulletStormActive, this);

        OnRep_GlobalBulletStorm();
    }
}

void ALastStandLegacyGameState::StartDoublePoints(float Duration)
{
    if (HasAuthority())
    {
        bIsDoublePointsActive = true;
        MARK_PROPERTY_DIRTY_FROM_NAME(ALastStandLegacyGameState, bIsDoublePointsActive, this);

        OnRep_DoublePoints();
        GetWorldTimerManager().SetTimer(DoublePointTimer, this, &ALastStandLegacyGameState::EndDoublePoints, Duration, false);
    }
}

void ALastStandLegacyGameState::EndDoublePoints()
{
    if (HasAuthority())
    {
        bIsDoublePointsActive = false;
        MARK_PROPERTY_DIRTY_FROM_NAME(ALastStandLegacyGameState, bIsDoublePointsActive, this);

        OnRep_DoublePoints();
    }
}

void ALastStandLegacyGameState::StartinstaKill(float Duration)
{
    if (HasAuthority())
    {
        bHasInstaKill = true;
        MARK_PROPERTY_DIRTY_FROM_NAME(ALastStandLegacyGameState, bHasInstaKill, this);

        OnRep_InstaKill();
        GetWorldTimerManager().SetTimer(InstaKillTimer, this, &ALastStandLegacyGameState::EndInstaKill, Duration, false);
    }
}

void ALastStandLegacyGameState::EndInstaKill()
{
    if (HasAuthority())
    {
        bHasInstaKill = false;
        MARK_PROPERTY_DIRTY_FROM_NAME(ALastStandLegacyGameState, bHasInstaKill, this);

        OnRep_InstaKill();
    }
}

void ALastStandLegacyGameState::OnRep_Adrenaline()
{
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, bIsAdrenalineActive ? FColor::Orange : FColor::Red, bIsAdrenalineActive ? TEXT("CLIENT: Blitz Adrenaline Active!") : TEXT("CLIENT: Blitz Ended!"));
}

void ALastStandLegacyGameState::OnRep_DoublePoints()
{
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, bIsDoublePointsActive ? FColor::Yellow : FColor::Red, bIsDoublePointsActive ? TEXT("CLIENT: Double Points Active!") : TEXT("CLIENT: Double Points Ended!"));
}

void ALastStandLegacyGameState::OnRep_InstaKill()
{
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, bHasInstaKill ? FColor::Magenta : FColor::Red, bHasInstaKill ? TEXT("CLIENT: Insta-Kill Active!") : TEXT("CLIENT: Insta-Kill Ended!"));
}

void ALastStandLegacyGameState::OnRep_GlobalBulletStorm()
{
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, bIsGlobalBulletStormActive ? FColor::Green : FColor::Red, bIsGlobalBulletStormActive ? TEXT("CLIENT: BulletStorm is Active!") : TEXT("CLIENT: BulletStorm Ended!"));
}