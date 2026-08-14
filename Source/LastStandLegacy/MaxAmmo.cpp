#include "MaxAmmo.h"
#include "Hama.h"

AMaxAmmo::AMaxAmmo()
{
}

void AMaxAmmo::ActivatePowerUp(AHama* Player)
{
    if (!HasAuthority()) return;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (APlayerController* PC = It->Get())
        {
            if (AHama* PlayerPawn = Cast<AHama>(PC->GetPawn()))
            {
                PlayerPawn->RefillAllWeapons();
            }
        }
    }
}