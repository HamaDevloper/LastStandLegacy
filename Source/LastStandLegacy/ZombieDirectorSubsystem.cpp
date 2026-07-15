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
    }
}

void UZombieDirectorSubsystem::UnregisterPlayer(APawn* Player)
{
    int32 Index = ActivePlayers.Find(Player);
    if (Index != INDEX_NONE)
    {
        ActivePlayers.RemoveAtSwap(Index, 1, EAllowShrinking::No);
    }
}

void UZombieDirectorSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_Client) return;

    int32 ZombieCount = ActiveZombies.Num();
    if (ZombieCount == 0 || ActivePlayers.IsEmpty()) return;

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

        for (APawn* Player : ActivePlayers)
        {
            if (!IsValid(Player)) continue;

            float DistSq = FVector::DistSquared(ZombieLoc, Player->GetActorLocation());
            if (DistSq < ClosestDistanceSq)
            {
                ClosestDistanceSq = DistSq;
                NearestPlayer = Player;
            }
        }

        if (NearestPlayer)
        {
            if (AAIController* AICon = Zombie->CachedAIController)
            {
                float TargetMovedDistSq = FVector::DistSquared(Zombie->LastTargetLocation, NearestPlayer->GetActorLocation());

                if (Zombie->CurrentTarget != NearestPlayer || TargetMovedDistSq > PathUpdateDistanceThresholdSq)
                {
                    Zombie->CurrentTarget = NearestPlayer;
                    Zombie->LastTargetLocation = NearestPlayer->GetActorLocation();

                    AICon->MoveToActor(NearestPlayer, 40.f, true, true, true);
                }
            }
        }
    }

    CurrentZombieIndex = EndIndex;
    if (CurrentZombieIndex >= ZombieCount)
    {
        CurrentZombieIndex = 0;
    }
}

TStatId UZombieDirectorSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UZombieDirectorSubsystem, STATGROUP_Tickables);
}