#include "MaxAmmo.h"
#include "Hama.h"
#include "ZombieDirectorSubsystem.h" 

AMaxAmmo::AMaxAmmo()
{
}

void AMaxAmmo::ActivatePowerUp(AHama* Player)
{
    if (!HasAuthority()) return;

    if (UZombieDirectorSubsystem* Director = GetWorld()->GetSubsystem<UZombieDirectorSubsystem>())
    {
        const TArray<TWeakObjectPtr<APawn>>& Players = Director->GetActivePlayers();

        for (const TWeakObjectPtr<APawn>& WeakPawn : Players)
        {
            if (APawn* Pawn = WeakPawn.Get()) 
            {
                if (AHama* Hama = Cast<AHama>(Pawn))
                {
                    Hama->RefillAllWeapons();
                }
            }
        }
    }
}