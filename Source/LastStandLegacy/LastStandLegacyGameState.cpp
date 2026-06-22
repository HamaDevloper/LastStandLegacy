#include "LastStandLegacyGameState.h"
#include "Net/UnrealNetwork.h"

ALastStandLegacyGameState::ALastStandLegacyGameState()
{
    SetNetUpdateFrequency(1.f); // بۆ گەیم ستەیت 1 زۆر کەمە ئەگەر شتی خێرا بگۆڕێت، بەڵام بۆ ئێستا ئاساییە.
}

void ALastStandLegacyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ALastStandLegacyGameState, bIsGlobalBulletStormActive);
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

// ئەمە تەنها سێرڤەر بانگی دەکات
void ALastStandLegacyGameState::StartGlobalBulletStorm(float Duration)
{
    if (HasAuthority())
    {
        bIsGlobalBulletStormActive = true;
        ForceNetUpdate();

        OnRep_GlobalBulletStorm();

        GetWorldTimerManager().SetTimer(BulletStormTimerHandle, this, &ALastStandLegacyGameState::EndGlobalBulletStorm, Duration, false);
    }
}

void ALastStandLegacyGameState::EndGlobalBulletStorm()
{
    if (HasAuthority())
    {
        bIsGlobalBulletStormActive = false;
        ForceNetUpdate();

        OnRep_GlobalBulletStorm();
        GetWorldTimerManager().ClearTimer(BulletStormTimerHandle);
    }
}