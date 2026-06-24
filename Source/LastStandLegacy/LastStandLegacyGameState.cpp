#include "LastStandLegacyGameState.h"
#include "Net/UnrealNetwork.h"

ALastStandLegacyGameState::ALastStandLegacyGameState()
{
    SetNetUpdateFrequency(1.f);
}

void ALastStandLegacyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ALastStandLegacyGameState, bIsGlobalBulletStormActive);
    DOREPLIFETIME(ALastStandLegacyGameState, bIsDoublePointsActive);
    DOREPLIFETIME(ALastStandLegacyGameState, bHasInstaKill);
}

// ئەمە تەنها سێرڤەر بانگی دەکات
void ALastStandLegacyGameState::StartGlobalBulletStorm(float Duration)
{
    if (HasAuthority())
    {
        bIsGlobalBulletStormActive = true;
        ForceNetUpdate();

        OnRep_GlobalBulletStorm();

        GetWorldTimerManager().SetTimer(BulletStormTimer, this, &ALastStandLegacyGameState::EndGlobalBulletStorm, Duration, false);
    }
}

void ALastStandLegacyGameState::EndGlobalBulletStorm()
{
    if (HasAuthority())
    {
        bIsGlobalBulletStormActive = false;
        ForceNetUpdate();

        OnRep_GlobalBulletStorm();
        GetWorldTimerManager().ClearTimer(BulletStormTimer);
    }
}

void ALastStandLegacyGameState::StartDoublePoints(float Duration)
{
    if (HasAuthority())
    {
        bIsDoublePointsActive = true;
        ForceNetUpdate();

        OnRep_DoublePoints();

        GetWorldTimerManager().SetTimer(DoublePointTimer, this, &ALastStandLegacyGameState::EndDoublePoints, Duration, false);
    }
}

void ALastStandLegacyGameState::EndDoublePoints()
{
    if (HasAuthority())
    {
        bIsDoublePointsActive = false;
        ForceNetUpdate();

        OnRep_DoublePoints();

        GetWorldTimerManager().ClearTimer(DoublePointTimer);
    }
}

void ALastStandLegacyGameState::StartinstaKill(float Duration)
{
    if (HasAuthority())
    {
        bHasInstaKill = true;
        ForceNetUpdate();

        OnRep_InstaKill();

        GetWorldTimerManager().SetTimer(InstaKillTimer, this, &ALastStandLegacyGameState::EndInstaKill, Duration, false);
    }
}

void ALastStandLegacyGameState::EndInstaKill()
{
    if (HasAuthority())
    {
        bHasInstaKill = false;
        ForceNetUpdate();

        OnRep_InstaKill();

        GetWorldTimerManager().ClearTimer(InstaKillTimer);
    }
}

void ALastStandLegacyGameState::OnRep_DoublePoints()
{
}

void ALastStandLegacyGameState::OnRep_InstaKill()
{
}

void ALastStandLegacyGameState::OnRep_GlobalBulletStorm()
{
    if (bIsGlobalBulletStormActive)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, TEXT("CLIENT: BulletStorm is Active!"));
        // لێرەدا دەنگ یان ئیفێکتی دەستپێکردن لێبدە
    }
    else
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("CLIENT: BulletStorm Ended!"));
        // لێرەدا دەنگ یان ئیفێکتەکان بوەستێنە
    }
}