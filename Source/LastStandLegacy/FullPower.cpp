#include "FullPower.h"
#include "Hama.h"
#include "HamaAbilityComponent.h"

AFullPower::AFullPower()
{
}

void AFullPower::ActivatePowerUp(AHama* Player)
{
    if (Player)
    {
        if (UHamaAbilityComponent* Comp = Player->FindComponentByClass<UHamaAbilityComponent>())
        {
            Comp->FullPower();
        }
    }
}