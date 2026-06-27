#include "Instakill.h"
#include "Hama.h"
#include "LastStandLegacyGameState.h"

AInstakill::AInstakill()
{
}

void AInstakill::ActivatePowerUp(AHama* Player)
{
    if (ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>())
    {
        GS->StartinstaKill(Duration);
    }
}