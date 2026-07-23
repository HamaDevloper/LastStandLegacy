#include "ZombieDirectorSubsystem.h"
#include "Zombie.h"
#include "AIController.h"

void UZombieDirectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UZombieDirectorSubsystem::Deinitialize()
{
    ActiveZombies.Empty();
    ActivePlayers.Empty();
    Super::Deinitialize();
}

void UZombieDirectorSubsystem::RegisterZombie(AZombie* Zombie)
{
    if (Zombie && !ActiveZombies.Contains(Zombie))
    {
        ActiveZombies.Add(Zombie);
    }
}

void UZombieDirectorSubsystem::UnregisterZombie(AZombie* Zombie)
{
    int32 Index = ActiveZombies.Find(Zombie);
    if (Index != INDEX_NONE)
    {
        ActiveZombies.RemoveAtSwap(Index, 1, EAllowShrinking::No);

        if (Index <= CurrentZombieIndex && CurrentZombieIndex > 0)
        {
            CurrentZombieIndex--;
        }
    }
}

void UZombieDirectorSubsystem::RegisterPlayer(APawn* Player)
{
    if (Player && !ActivePlayers.Contains(Player))
    {
        ActivePlayers.Add(Player);
        SetPlayerTargetable(Player, true);
    }
}

void UZombieDirectorSubsystem::UnregisterPlayer(APawn* Player)
{
    int32 Index = ActivePlayers.Find(Player);

    SetPlayerTargetable(Player, false);

    if (Index != INDEX_NONE)
    {
        ActivePlayers.RemoveAtSwap(Index, 1, EAllowShrinking::No);
    }
}

void UZombieDirectorSubsystem::SetPlayerTargetable(APawn* Player, bool bIsTargetable)
{
    if (!Player) return;

    if (bIsTargetable)
    {
        if (!ValidTargetPlayers.Contains(Player)) ValidTargetPlayers.Add(Player);
    }
    else
    {
        ValidTargetPlayers.RemoveSingleSwap(Player, EAllowShrinking::No);
    }
}

void UZombieDirectorSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (ActiveZombies.IsEmpty()) return;

    int32 ZombieCount = ActiveZombies.Num();

    constexpr int32 DesiredCycleFrames = 30;
    ZombiesToUpdatePerFrame = FMath::Clamp(FMath::CeilToInt(ZombieCount / (float)DesiredCycleFrames), 1, 25);

    int32 EndIndex = FMath::Min(CurrentZombieIndex + ZombiesToUpdatePerFrame, ZombieCount);

    for (int32 i = CurrentZombieIndex; i < EndIndex; ++i)
    {
        AZombie* Zombie = ActiveZombies[i];
        if (!IsValid(Zombie) || Zombie->bIsDead) continue;

        APawn* NearestPlayer = nullptr;
        float ClosestDistanceSq = UE_BIG_NUMBER;
        const FVector ZombieLoc = Zombie->GetActorLocation();

        // تەنها گەڕان کاتێک یاریزانی ڕەوا هەیە
        if (!ValidTargetPlayers.IsEmpty())
        {
            for (APawn* TargetPlayer : ValidTargetPlayers)
            {
                if (!IsValid(TargetPlayer)) continue;

                float DistSq = FVector::DistSquared(ZombieLoc, TargetPlayer->GetActorLocation());
                if (DistSq < ClosestDistanceSq)
                {
                    ClosestDistanceSq = DistSq;
                    NearestPlayer = TargetPlayer;
                }
            }
        }

        AAIController* AICon = Zombie->CachedAIController;
        if (!AICon) continue;

        if (NearestPlayer)
        {
            float TargetMovedDistSq = FVector::DistSquared(Zombie->LastTargetLocation, NearestPlayer->GetActorLocation());
            if (Zombie->CurrentTarget != NearestPlayer || TargetMovedDistSq > PathUpdateDistanceThresholdSq)
            {
                Zombie->CurrentTarget = NearestPlayer;
                Zombie->LastTargetLocation = NearestPlayer->GetActorLocation();
                AICon->MoveToActor(NearestPlayer, 40.f, true, true, true);
            }
        }
        else
        {
            if (Zombie->CurrentTarget != nullptr)
            {
                Zombie->CurrentTarget = nullptr;
                AICon->StopMovement();
            }
        }
    }

    CurrentZombieIndex = EndIndex;
    if (CurrentZombieIndex >= ZombieCount) CurrentZombieIndex = 0;
}

TStatId UZombieDirectorSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UZombieDirectorSubsystem, STATGROUP_Tickables);
}