#include "Nuke.h"
#include "Hama.h"
#include "LastStandLegacyGameMode.h"

ANuke::ANuke()
{
}

void ANuke::ActivatePowerUp(AHama* Player)
{
    if (ALastStandLegacyGameMode* GM = GetWorld()->GetAuthGameMode<ALastStandLegacyGameMode>())
    {
        GM->ActivateNuke();
    }
}