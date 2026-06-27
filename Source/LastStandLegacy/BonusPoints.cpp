#include "BonusPoints.h"
#include "Hama.h"
#include "HamaPlayerState.h"

ABonusPoints::ABonusPoints()
{
}

void ABonusPoints::ActivatePowerUp(AHama* Player)
{
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (APlayerController* PC = It->Get())
        {
            if (AHamaPlayerState* PS = PC->GetPlayerState<AHamaPlayerState>())
            {
                PS->AddPoints(AddPoints);
            }
        }
    }
}