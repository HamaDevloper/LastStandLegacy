#include "DoublePoint.h"
#include "Hama.h"
#include "LastStandLegacyGameState.h"

ADoublePoint::ADoublePoint()
{
}

void ADoublePoint::ActivatePowerUp(AHama* Player)
{
    if (ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>())
    {
        GS->StartDoublePoints(Duration);
    }
}