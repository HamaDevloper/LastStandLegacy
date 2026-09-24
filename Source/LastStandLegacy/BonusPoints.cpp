#include "BonusPoints.h"
#include "Hama.h"
#include "HamaPlayerState.h"
#include "LastStandLegacyGameState.h"

ABonusPoints::ABonusPoints()
{
}

void ABonusPoints::ActivatePowerUp(AHama* Player)
{
    if(ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>())
    {
        for (APlayerState* PlayerState : GS->PlayerArray)
        {
            if (!PlayerState) continue;
            if (AHamaPlayerState* HamaPS = Cast<AHamaPlayerState>(PlayerState))
            {
                HamaPS->AddPoints(AddPoints);
            }
        }
    }
}