#include "BonusPoints.h"
#include "Hama.h"
#include "HamaPlayerState.h"
#include "ZombieDirectorSubsystem.h"

ABonusPoints::ABonusPoints()
{
}

void ABonusPoints::ActivatePowerUp(AHama* Player)
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
                    if (AHamaPlayerState* HamaPS = Hama->GetPlayerState<AHamaPlayerState>())
                    {
                        HamaPS->AddPoints(AddPoints);
                    }
                }
            }
        }
    }
}