#include "ZombieDirectorSubsystem.h"
#include "Zombie.h"
#include "AIController.h"
#include "MysteryBoxSpawnPoint.h" 
#include "Navigation/PathFollowingComponent.h"

void UZombieDirectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UZombieDirectorSubsystem::Deinitialize()
{
    ActiveZombies.Empty();
    ActivePlayers.Empty();
    ValidTargetPlayers.Empty();
    CachedPlayers.Empty();
    CachedPlayerLocations.Empty();
    MysteryBoxSpawnPoints.Empty();
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

    if (GetWorld()->GetNetMode() == NM_Client) return;
    if (ActiveZombies.IsEmpty() || ValidTargetPlayers.IsEmpty()) return;

    const float CurrentTime = GetWorld()->GetTimeSeconds();

    // ١. نوێکردنەوەی کاشی یاریزانەکان
    PlayerCacheRefreshTimer -= DeltaTime;
    if (PlayerCacheRefreshTimer <= 0.f)
    {
        PlayerCacheRefreshTimer = 0.1f;
        CachedPlayers.Reset();
        CachedPlayerLocations.Reset();

        for (APawn* P : ValidTargetPlayers)
        {
            if (IsValid(P))
            {
                CachedPlayers.Add(P);
                CachedPlayerLocations.Add(P->GetActorLocation());
            }
        }
    }

    if (CachedPlayers.IsEmpty()) return;

    int32 ZombieCount = ActiveZombies.Num();
    if (CurrentZombieIndex >= ZombieCount)
    {
        CurrentZombieIndex = 0;
    }

    constexpr int32 DesiredCycleFrames = 30;
    ZombiesToUpdatePerFrame = FMath::Clamp(FMath::CeilToInt(ZombieCount / (float)DesiredCycleFrames), 1, 30);

    static constexpr float TargetMovedThresholdSq = 150.f * 150.f;
    const double StartTime = FPlatformTime::Seconds();
    constexpr double TimeBudgetSeconds = 0.0005; // 0.5ms Time-Budget

    int32 i = CurrentZombieIndex;
    for (; i < ZombieCount; ++i)
    {
        AZombie* Zombie = ActiveZombies[i].Get();
        if (!IsValid(Zombie) || Zombie->bIsDead) continue;

        const FVector ZombieLoc = Zombie->GetActorLocation();
        APawn* TargetPlayer = nullptr;
        FVector PlayerLoc = FVector::ZeroVector;

        // ⚡ [FIX]: پشکنینی کۆڵدۆن بە کاتی ڕاستەقینەی ناو یاری
        bool bNeedsSearch = (CurrentTime >= Zombie->NextTargetSearchTime || !IsValid(Zombie->CurrentTarget));

        if (!bNeedsSearch && Zombie->CurrentTarget)
        {
            int32 CurrentTargetIdx = CachedPlayers.IndexOfByKey(Zombie->CurrentTarget);
            if (CurrentTargetIdx != INDEX_NONE)
            {
                TargetPlayer = Zombie->CurrentTarget;
                PlayerLoc = CachedPlayerLocations[CurrentTargetIdx];
            }
            else
            {
                bNeedsSearch = true;
            }
        }

        if (bNeedsSearch)
        {
            int32 NearestPlayerIndex = INDEX_NONE;
            float ClosestDistanceSq = UE_BIG_NUMBER;

            for (int32 p = 0; p < CachedPlayers.Num(); ++p)
            {
                float DistSq = FVector::DistSquared(ZombieLoc, CachedPlayerLocations[p]);
                if (DistSq < ClosestDistanceSq)
                {
                    ClosestDistanceSq = DistSq;
                    NearestPlayerIndex = p;
                }
            }

            if (NearestPlayerIndex != INDEX_NONE)
            {
                TargetPlayer = CachedPlayers[NearestPlayerIndex].Get();
                PlayerLoc = CachedPlayerLocations[NearestPlayerIndex];

                // ⚡ [SIGNIFICANCE Tiers]: دیاری کردنی داهاتووی پشکنین بەپێی دووری (ClosestDistanceSq)
                float NextInterval = 0.25f; // High Significance (< 10m)

                if (ClosestDistanceSq > 9000000.f) // > 30 meters
                {
                    NextInterval = 2.0f; // Low Significance (پشکنین دوای 2 چڕکە)
                }
                else if (ClosestDistanceSq > 1000000.f) // > 10 meters
                {
                    NextInterval = 0.75f; // Medium Significance
                }

                Zombie->NextTargetSearchTime = CurrentTime + NextInterval + FMath::RandRange(0.0f, 0.1f);
            }
        }

        AAIController* AICon = Zombie->CachedAIController;
        if (!AICon) continue;

        if (TargetPlayer)
        {
            float TargetMovedDistSq = FVector::DistSquared(Zombie->LastTargetLocation, PlayerLoc);
            bool bIsAIIdle = AICon->GetMoveStatus() == EPathFollowingStatus::Idle;

            if (Zombie->CurrentTarget != TargetPlayer || TargetMovedDistSq > TargetMovedThresholdSq || bIsAIIdle)
            {
                Zombie->CurrentTarget = TargetPlayer;
                Zombie->LastTargetLocation = PlayerLoc;

                // ⚡ MoveToActor تەنها لە شۆفێری پێویستدا بانگ دەکرێت
                AICon->MoveToActor(TargetPlayer, 40.f, true, true, true);
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

        if ((FPlatformTime::Seconds() - StartTime) > TimeBudgetSeconds)
        {
            ++i;
            break;
        }
    }

    CurrentZombieIndex = (i >= ZombieCount) ? 0 : i;
}

TStatId UZombieDirectorSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UZombieDirectorSubsystem, STATGROUP_Tickables);
}

// ==========================================
// ⚡ Mystery Box Spawn Point Management
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
    }
}

AMysteryBoxSpawnPoint* UZombieDirectorSubsystem::GetRandomFreeMysteryBoxPoint(AMysteryBoxSpawnPoint* CurrentPoint)
{
    TArray<AMysteryBoxSpawnPoint*> ValidPoints;

    for (AMysteryBoxSpawnPoint* Point : MysteryBoxSpawnPoints)
    {
        if (IsValid(Point) && Point != CurrentPoint && !Point->IsOccupied())
        {
            ValidPoints.Add(Point);
        }
    }

    if (ValidPoints.Num() > 0)
    {
        int32 RandomIndex = FMath::RandRange(0, ValidPoints.Num() - 1);
        return ValidPoints[RandomIndex];
    }

    return nullptr;
}