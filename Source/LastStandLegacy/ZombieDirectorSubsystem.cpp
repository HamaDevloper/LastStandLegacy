#include "ZombieDirectorSubsystem.h"
#include "Zombie.h"
#include "AIController.h"
#include "MysteryBoxSpawnPoint.h" 
#include "Navigation/PathFollowingComponent.h"
#include "Engine/World.h"

void UZombieDirectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UZombieDirectorSubsystem::Deinitialize()
{
    ActiveZombies.Empty();
    ActivePlayers.Empty();
    ValidTargetPlayers.Empty();
    MysteryBoxSpawnPoints.Empty();
    ReusableMysteryBoxPoints.Empty();
    CachedPlayerMap.Reset();
    SpatialGrid.Reset();

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
    if (!Zombie) return;

    const int32 RemovedCount = ActiveZombies.RemoveAllSwap(
        [Zombie](const TWeakObjectPtr<AZombie>& Item)
        {
            return !Item.IsValid() || Item.Get() == Zombie;
        },
        EAllowShrinking::No
    );

    if (RemovedCount > 0 && CurrentZombieIndex >= ActiveZombies.Num())
    {
        CurrentZombieIndex = 0;
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
    if (!Player) return;
    SetPlayerTargetable(Player, false);
    ActivePlayers.RemoveSwap(Player, EAllowShrinking::No);
}

void UZombieDirectorSubsystem::SetPlayerTargetable(APawn* Player, bool bIsTargetable)
{
    if (!Player) return;

    if (bIsTargetable)
    {
        if (!ValidTargetPlayers.Contains(Player))
        {
            ValidTargetPlayers.Add(Player);
        }
    }
    else
    {
        ValidTargetPlayers.RemoveAllSwap(
            [Player](const TWeakObjectPtr<APawn>& Item)
            {
                return !Item.IsValid() || Item.Get() == Player;
            },
            EAllowShrinking::No
        );

        CachedPlayerMap.Remove(Player);
    }
}

void UZombieDirectorSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (GetWorld()->GetNetMode() == NM_Client) return;
    if (ActiveZombies.IsEmpty()) return;

    if (ValidTargetPlayers.IsEmpty())
    {
        for (const TWeakObjectPtr<AZombie>& ZombieWeak : ActiveZombies)
        {
            AZombie* Zombie = ZombieWeak.Get();
            if (!IsValid(Zombie) || Zombie->IsDead()) continue;

            if (Zombie->CurrentTarget != nullptr)
            {
                Zombie->CurrentTarget = nullptr;
                if (Zombie->CachedAIController)
                {
                    Zombie->CachedAIController->StopMovement();
                }
            }
        }
        return;
    }

    PlayerCacheRefreshTimer -= DeltaTime;

    if (PlayerCacheRefreshTimer <= 0.f)
    {
        PlayerCacheRefreshTimer = 0.2f;

        CachedPlayerMap.Reset();
        SpatialGrid.Reset();

        for (int32 i = ValidTargetPlayers.Num() - 1; i >= 0; --i)
        {
            if (ValidTargetPlayers[i].IsValid())
            {
                APawn* P = ValidTargetPlayers[i].Get();
                if (IsValid(P))
                {
                    const FVector Loc = P->GetActorLocation();
                    CachedPlayerMap.Add(P, Loc);
                    SpatialGrid.InsertPlayer(P, Loc);
                }
            }
            else
            {
                ValidTargetPlayers.RemoveAtSwap(i, 1, EAllowShrinking::No);
            }
        }

        for (int32 i = ActiveZombies.Num() - 1; i >= 0; --i)
        {
            AZombie* Z = ActiveZombies[i].Get();
            if (!IsValid(Z) || Z->IsDead())
            {
                ActiveZombies.RemoveAtSwap(i, 1, EAllowShrinking::No);
            }
        }

        if (CurrentZombieIndex >= ActiveZombies.Num())
        {
            CurrentZombieIndex = 0;
        }
    }

    if (CachedPlayerMap.IsEmpty() || ActiveZombies.IsEmpty()) return;

    const float CurrentTime = GetWorld()->GetTimeSeconds();
    const int32 ZombieCount = ActiveZombies.Num();

    const double DynamicTimeBudget = FMath::Clamp(DeltaTime * 0.04f, 0.0003f, 0.0015f);
    const double StartTime = FPlatformTime::Seconds();
    static constexpr float TargetMovedThresholdSq = 180.f * 180.f;

    int32 MaxRepathRequestsThisFrame = 3;
    int32 CurrentRepathCount = 0;

    int32 i = CurrentZombieIndex;
    for (; i < ZombieCount; ++i)
    {
        if ((FPlatformTime::Seconds() - StartTime) > DynamicTimeBudget)
        {
            break;
        }

        AZombie* Zombie = ActiveZombies[i].Get();
        if (!IsValid(Zombie) || Zombie->IsDead()) continue;

        const FVector ZombieLoc = Zombie->GetActorLocation();
        APawn* TargetPlayer = nullptr;
        FVector PlayerLoc = FVector::ZeroVector;

        bool bNeedsSearch = (CurrentTime >= Zombie->NextTargetSearchTime || !IsValid(Zombie->CurrentTarget));

        if (!bNeedsSearch && Zombie->CurrentTarget)
        {
            if (const FVector* FoundLoc = CachedPlayerMap.Find(Zombie->CurrentTarget))
            {
                TargetPlayer = Zombie->CurrentTarget;
                PlayerLoc = *FoundLoc;
            }
            else
            {
                bNeedsSearch = true;
            }
        }

        if (bNeedsSearch)
        {
            float ClosestDistanceSq = UE_BIG_NUMBER;
            TargetPlayer = SpatialGrid.FindNearestPlayerInRadius(ZombieLoc, ClosestDistanceSq);

            if (TargetPlayer)
            {
                if (const FVector* FoundLoc = CachedPlayerMap.Find(TargetPlayer))
                {
                    PlayerLoc = *FoundLoc;
                }
                else
                {
                    TargetPlayer = nullptr;
                }
            }

            if (!TargetPlayer)
            {
                for (const TPair<APawn*, FVector>& Pair : CachedPlayerMap)
                {
                    APawn* CandidatePlayer = Pair.Key;
                    if (!IsValid(CandidatePlayer)) continue;

                    const float DistSq = FVector::DistSquared(ZombieLoc, Pair.Value);
                    if (DistSq < ClosestDistanceSq)
                    {
                        ClosestDistanceSq = DistSq;
                        TargetPlayer = CandidatePlayer;
                    }
                }
            }

            if (TargetPlayer)
            {
                float NextInterval = 0.25f;
                if (ClosestDistanceSq > 16000000.f)
                {
                    NextInterval = 1.8f;
                }
                else if (ClosestDistanceSq > 1000000.f)
                {
                    NextInterval = 0.6f;
                }

                Zombie->NextTargetSearchTime = CurrentTime + NextInterval + FMath::RandRange(0.0f, 0.1f);
            }
        }

        AAIController* AICon = Zombie->CachedAIController;
        if (!AICon) continue;

        if (TargetPlayer)
        {
            const float TargetMovedDistSq = FVector::DistSquared(Zombie->LastTargetLocation, PlayerLoc);
            const bool bIsAIIdle = AICon->GetMoveStatus() == EPathFollowingStatus::Idle;

            const bool bShouldRepath = (Zombie->CurrentTarget != TargetPlayer) ||
                (TargetMovedDistSq > TargetMovedThresholdSq) ||
                (bIsAIIdle && CurrentTime >= Zombie->NextForceRepathTime);

            if (bShouldRepath && CurrentRepathCount < MaxRepathRequestsThisFrame)
            {
                Zombie->CurrentTarget = TargetPlayer;
                Zombie->LastTargetLocation = PlayerLoc;

                if (bIsAIIdle)
                {
                    Zombie->NextForceRepathTime = CurrentTime + 0.5f;
                }

                FAIMoveRequest MoveReq(TargetPlayer);
                MoveReq.SetAcceptanceRadius(40.f);
                MoveReq.SetUsePathfinding(true);

                AICon->MoveTo(MoveReq);

                CurrentRepathCount++;
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

    CurrentZombieIndex = (i >= ZombieCount) ? 0 : i;
}

TStatId UZombieDirectorSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UZombieDirectorSubsystem, STATGROUP_Tickables);
}

// ==========================================
// Mystery Box Spawn Point Management
// ==========================================

void UZombieDirectorSubsystem::RegisterMysteryBoxSpawnPoint(AMysteryBoxSpawnPoint* Point)
{
    if (Point && !MysteryBoxSpawnPoints.Contains(Point))
    {
        MysteryBoxSpawnPoints.Add(Point);
    }
}

void UZombieDirectorSubsystem::UnregisterMysteryBoxSpawnPoint(AMysteryBoxSpawnPoint* Point)
{
    if (Point)
    {
        MysteryBoxSpawnPoints.RemoveSingleSwap(Point, EAllowShrinking::No);
        ReusableMysteryBoxPoints.RemoveSingleSwap(Point, EAllowShrinking::No);
    }
}

AMysteryBoxSpawnPoint* UZombieDirectorSubsystem::GetRandomFreeMysteryBoxPoint(AMysteryBoxSpawnPoint* CurrentPoint)
{
    ReusableMysteryBoxPoints.Reset();

    for (AMysteryBoxSpawnPoint* Point : MysteryBoxSpawnPoints)
    {
        if (IsValid(Point) && Point != CurrentPoint && !Point->IsOccupied())
        {
            ReusableMysteryBoxPoints.Add(Point);
        }
    }

    if (ReusableMysteryBoxPoints.Num() > 0)
    {
        int32 RandomIndex = FMath::RandRange(0, ReusableMysteryBoxPoints.Num() - 1);
        return ReusableMysteryBoxPoints[RandomIndex];
    }

    return nullptr;
}