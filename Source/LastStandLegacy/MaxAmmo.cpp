#include "MaxAmmo.h"
#include "Hama.h"
#include "LastStandLegacyGameState.h"

AMaxAmmo::AMaxAmmo()
{
}

void AMaxAmmo::ActivatePowerUp(AHama* Player)
{
    if (!HasAuthority()) return;

    if (ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>())
    {
        for (APlayerState* PlayerState : GS->PlayerArray)
        {
            if (!PlayerState) continue;
            if(AHama* Hama = Cast<AHama>(PlayerState->GetPawn()))
            {
                Hama->RefillAllWeapons();
            }
        }
    }
}