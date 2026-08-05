#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

struct FSpatialHashGrid2D
{
    float CellSize = 2000.f;

    TMap<FIntPoint, TArray<TWeakObjectPtr<APawn>>> Grid;

    FORCEINLINE FIntPoint GetCellCoords(const FVector& WorldLocation) const
    {
        return FIntPoint(
            FMath::FloorToInt(WorldLocation.X / CellSize),
            FMath::FloorToInt(WorldLocation.Y / CellSize)
        );
    }

    void Reset()
    {
        Grid.Reset();
    }

    void InsertPlayer(APawn* Player, const FVector& Location)
    {
        if (!IsValid(Player)) return;
        const FIntPoint Coords = GetCellCoords(Location);
        Grid.FindOrAdd(Coords).Add(Player);
    }

    APawn* FindNearestPlayerInRadius(const FVector& ZombieLocation, float& OutDistSq) const
    {
        const FIntPoint ZombieCell = GetCellCoords(ZombieLocation);
        APawn* ClosestPlayer = nullptr;
        OutDistSq = UE_BIG_NUMBER;

        for (int32 x = -1; x <= 1; ++x)
        {
            for (int32 y = -1; y <= 1; ++y)
            {
                const FIntPoint TargetCell = ZombieCell + FIntPoint(x, y);
                if (const TArray<TWeakObjectPtr<APawn>>* PlayersInCell = Grid.Find(TargetCell))
                {
                    for (const TWeakObjectPtr<APawn>& WeakPlayer : *PlayersInCell)
                    {
                        if (APawn* Player = WeakPlayer.Get())
                        {
                            if (!IsValid(Player)) continue;

                            const float DistSq = FVector::DistSquared(ZombieLocation, Player->GetActorLocation());
                            if (DistSq < OutDistSq)
                            {
                                OutDistSq = DistSq;
                                ClosestPlayer = Player;
                            }
                        }
                    }
                }
            }
        }

        return ClosestPlayer;
    }
};