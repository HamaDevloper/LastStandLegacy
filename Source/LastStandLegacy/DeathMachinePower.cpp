#include "DeathMachinePower.h"
#include "Hama.h"
#include "BaseWeapon.h"

void ADeathMachinePower::ActivatePowerUp(AHama* Player)
{
    if (Player && MachineGunClass)
    {
        Player->GiveDeathMachine(MachineGunClass, Duration);
    }
}