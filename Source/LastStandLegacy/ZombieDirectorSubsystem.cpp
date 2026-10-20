#include "ZombieDirectorSubsystem.h"
#include "Zombie.h"
#include "AIController.h"
#include "MysteryBoxSpawnPoint.h" 
#include "Navigation/PathFollowingComponent.h"
#include "Engine/World.h"
#include "MysteryBox.h"
#include "Kismet/GameplayStatics.h"

void UZombieDirectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UZombieDirectorSubsystem::Deinitialize()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(TimerHandle_FireSale);
    }

    ActiveZombies.Empty();
    ActivePlayers.Empty();
    ValidTargetPlayers.Empty();
    MysteryBoxSpawnPoints.Empty();
    ReusableMysteryBoxPoints.Empty();
    RegisteredBoxes.Empty();
    TempFireSaleBoxes.Empty();
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

// ==========================================
// Attractor / Monkey Bomb Registration
// ==========================================

void UZombieDirectorSubsystem::RegisterAttractor(AActor* AttractorActor, float Radius)
{
    if (!AttractorActor || GetWorld()->GetNetMode() == NM_Client) return;

    const bool bAlreadyExists = ActiveAttractors.ContainsByPredicate(
        [AttractorActor](const FAttractorData& Data)
        {
            return Data.AttractorActor.Get() == AttractorActor;
        });

    if (!bAlreadyExists)
    {
        FAttractorData NewAttractor;
        NewAttractor.AttractorActor = AttractorActor;
        NewAttractor.RadiusSq = FMath::Square(Radius);

        ActiveAttractors.Add(NewAttractor);
    }
}

void UZombieDirectorSubsystem::UnregisterAttractor(AActor* AttractorActor)
{
    if (!AttractorActor || GetWorld()->GetNetMode() == NM_Client) return;

    ActiveAttractors.RemoveAllSwap(
        [AttractorActor](const FAttractorData& Data)
        {
            return !Data.AttractorActor.IsValid() || Data.AttractorActor.Get() == AttractorActor;
        },
        EAllowShrinking::No
    );
}

void UZombieDirectorSubsystem::CleanInvalidAttractors()
{
    ActiveAttractors.RemoveAllSwap(
        [](const FAttractorData& Data)
        {
            return !Data.AttractorActor.IsValid();
        },
        EAllowShrinking::No
    );
}

void UZombieDirectorSubsystem::InitializeZombiePool(TSubclassOf<AZombie> InZombieClass, int32 PoolSize)
{
    UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_Client || !InZombieClass) return;

    PooledZombieClass = InZombieClass;
    InactiveZombiePool.Reserve(PoolSize);

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    const FVector PoolHoldingLocation(0.f, 0.f, -10000.f);

    for (int32 i = 0; i < PoolSize; ++i)
    {
        AZombie* NewZombie = World->SpawnActor<AZombie>(PooledZombieClass, PoolHoldingLocation, FRotator::ZeroRotator, SpawnParams);
        if (NewZombie)
        {
            NewZombie->InitializeForPool();
            InactiveZombiePool.Add(NewZombie);
        }
    }
}

AZombie* UZombieDirectorSubsystem::AcquireZombieFromPool(const FVector& SpawnLocation, const FRotator& SpawnRotation, int32 CurrentRound)
{
    UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_Client) return nullptr;

    while (!InactiveZombiePool.IsEmpty())
    {
        AZombie* PooledZombie = InactiveZombiePool.Pop(EAllowShrinking::No);
        if (IsValid(PooledZombie))
        {
            PooledZombie->ActivateFromPool(SpawnLocation, SpawnRotation, CurrentRound);
            return PooledZombie;
        }
    }

    if (PooledZombieClass)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

        AZombie* ExtraZombie = World->SpawnActor<AZombie>(PooledZombieClass, SpawnLocation, SpawnRotation, SpawnParams);
        if (ExtraZombie)
        {
            ExtraZombie->ActivateFromPool(SpawnLocation, SpawnRotation, CurrentRound);
            return ExtraZombie;
        }
    }

    return nullptr;
}

void UZombieDirectorSubsystem::ReturnZombieToPool(AZombie* Zombie)
{
    if (!IsValid(Zombie)) return;

    if (!InactiveZombiePool.Contains(Zombie))
    {
        InactiveZombiePool.Add(Zombie);
    }
}

void UZombieDirectorSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_Client) return;
    if (ActiveZombies.IsEmpty()) return;

    CleanInvalidAttractors();

    const bool bHasAttractors = !ActiveAttractors.IsEmpty();

    if (ValidTargetPlayers.IsEmpty() && !bHasAttractors)
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
                    Zombie->CachedAIController->ClearFocus(EAIFocusPriority::Gameplay);
                }
            }
        }
        return;
    }

    PlayerCacheRefreshTimer -= DeltaTime;
    if (PlayerCacheRefreshTimer <= 0.f)
    {
        PlayerCacheRefreshTimer = 0.05f;

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

    const float CurrentTime = World->GetTimeSeconds();
    const int32 ZombieCount = ActiveZombies.Num();

    const double DynamicTimeBudget = FMath::Clamp(DeltaTime * 0.06f, 0.0004f, 0.0020f);
    const double StartTime = FPlatformTime::Seconds();

    const int32 MaxRepathRequestsThisFrame = FMath::Clamp(ZombieCount / 4, 4, 10);
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

        AActor* FinalTargetActor = nullptr;
        FVector FinalTargetLoc = FVector::ZeroVector;
        float ClosestDistanceSq = UE_BIG_NUMBER;

        // =========================================================
        // A. ئەولەویەت بۆ مەیموونەکە (Attractor Priority)
        // =========================================================
        if (bHasAttractors)
        {
            for (const FAttractorData& Attractor : ActiveAttractors)
            {
                if (AActor* AttractorActor = Attractor.AttractorActor.Get())
                {
                    const FVector CurrentLoc = AttractorActor->GetActorLocation();
                    const float DistSq = FVector::DistSquared(ZombieLoc, CurrentLoc);

                    if (DistSq <= Attractor.RadiusSq && DistSq < ClosestDistanceSq)
                    {
                        ClosestDistanceSq = DistSq;
                        FinalTargetActor = AttractorActor;
                        FinalTargetLoc = CurrentLoc;
                    }
                }
            }
        }

        // =========================================================
        // B. دیاریکردنی یاریزان و وەرگرتنی شوێنی ڕاستەقینەی ئێستا (Live Location)
        // =========================================================
        if (!FinalTargetActor)
        {
            bool bNeedsSearch = (CurrentTime >= Zombie->NextTargetSearchTime || !IsValid(Zombie->CurrentTarget));

            if (!bNeedsSearch && Zombie->CurrentTarget)
            {
                if (APawn* TargetPawn = Cast<APawn>(Zombie->CurrentTarget))
                {
                    if (CachedPlayerMap.Contains(TargetPawn))
                    {
                        FinalTargetActor = TargetPawn;
                        FinalTargetLoc = TargetPawn->GetActorLocation();
                        ClosestDistanceSq = FVector::DistSquared(ZombieLoc, FinalTargetLoc);
                    }
                    else
                    {
                        bNeedsSearch = true;
                    }
                }
                else
                {
                    bNeedsSearch = true;
                }
            }

            if (bNeedsSearch)
            {
                APawn* TargetPlayer = SpatialGrid.FindNearestPlayerInRadius(ZombieLoc, ClosestDistanceSq);

                if (TargetPlayer && IsValid(TargetPlayer))
                {
                    FinalTargetActor = TargetPlayer;
                    FinalTargetLoc = TargetPlayer->GetActorLocation();
                    ClosestDistanceSq = FVector::DistSquared(ZombieLoc, FinalTargetLoc);
                }

                if (!FinalTargetActor)
                {
                    for (const auto& Pair : CachedPlayerMap)
                    {
                        APawn* CandidatePlayer = Pair.Key.Get();
                        if (!IsValid(CandidatePlayer)) continue;

                        const FVector RealLoc = CandidatePlayer->GetActorLocation();
                        const float DistSq = FVector::DistSquared(ZombieLoc, RealLoc);
                        if (DistSq < ClosestDistanceSq)
                        {
                            ClosestDistanceSq = DistSq;
                            FinalTargetActor = CandidatePlayer;
                            FinalTargetLoc = RealLoc;
                        }
                    }
                }

                if (FinalTargetActor)
                {
                    float NextInterval = 0.2f;
                    if (ClosestDistanceSq > 16000000.f) NextInterval = 1.2f;
                    else if (ClosestDistanceSq > 2250000.f) NextInterval = 0.5f;

                    Zombie->NextTargetSearchTime = CurrentTime + NextInterval + FMath::RandRange(0.0f, 0.08f);
                }
            }
        }

        // =========================================================
        // C. Dynamic Network LOD & AI Repathing
        // =========================================================
        AAIController* AICon = Zombie->CachedAIController;
        if (!AICon) continue;

        if (FinalTargetActor)
        {
            float DesiredNetFreq = 4.f;
            if (ClosestDistanceSq < 1440000.f)
            {
                DesiredNetFreq = 30.f;
            }
            else if (ClosestDistanceSq < 6250000.f)
            {
                DesiredNetFreq = 12.f;
            }

            if (!FMath::IsNearlyEqual(Zombie->GetNetUpdateFrequency(), DesiredNetFreq))
            {
                Zombie->SetNetUpdateFrequency(DesiredNetFreq);
            }

            if (ClosestDistanceSq < 250000.f)
            {
                if (AICon->GetFocusActor() != FinalTargetActor)
                {
                    AICon->SetFocus(FinalTargetActor);
                }
            }
            else if (AICon->GetFocusActor() != nullptr)
            {
                AICon->ClearFocus(EAIFocusPriority::Gameplay);
            }

            float DynamicMovedThresholdSq = 160.f * 160.f;
            if (ClosestDistanceSq < 360000.f)
            {
                DynamicMovedThresholdSq = 25.f * 25.f;
            }
            else if (ClosestDistanceSq < 2250000.f)
            {
                DynamicMovedThresholdSq = 75.f * 75.f;
            }

            const float TargetMovedDistSq = FVector::DistSquared(Zombie->LastTargetLocation, FinalTargetLoc);
            const bool bIsAIIdle = (AICon->GetMoveStatus() == EPathFollowingStatus::Idle);

            const bool bShouldRepath = (Zombie->CurrentTarget != FinalTargetActor) ||
                (TargetMovedDistSq > DynamicMovedThresholdSq) ||
                (bIsAIIdle && CurrentTime >= Zombie->NextForceRepathTime);

            if (bShouldRepath)
            {
                if (CurrentRepathCount >= MaxRepathRequestsThisFrame)
                {
                    break;
                }

                Zombie->CurrentTarget = FinalTargetActor;
                Zombie->LastTargetLocation = FinalTargetLoc;

                if (bIsAIIdle)
                {
                    Zombie->NextForceRepathTime = CurrentTime + 0.12f;
                }

                FAIMoveRequest MoveReq(FinalTargetActor);
                MoveReq.SetAcceptanceRadius(15.f); // کەمکرایەوە لە 40 بۆ 15 بۆ ئەوەی زوو نەوەستێت
                MoveReq.SetUsePathfinding(true);
                MoveReq.SetAllowPartialPath(true);
                MoveReq.SetProjectGoalLocation(true);

                AICon->MoveTo(MoveReq);

                CurrentRepathCount++;
            }
        }
        else
        {
            if (Zombie->CurrentTarget != nullptr)
            {
                Zombie->CurrentTarget = nullptr;
                AICon->ClearFocus(EAIFocusPriority::Gameplay);
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

void UZombieDirectorSubsystem::RegisterMysteryBox(AMysteryBox* Box)
{
    if (Box && !RegisteredBoxes.Contains(Box))
    {
        RegisteredBoxes.Add(Box);

        if (bIsFireSaleActive)
        {
            Box->SetFireSaleActive(true);
        }
    }
}

void UZombieDirectorSubsystem::UnregisterMysteryBox(AMysteryBox* Box)
{
    if (Box)
    {
        RegisteredBoxes.RemoveSingleSwap(Box, EAllowShrinking::No);
    }
}

void UZombieDirectorSubsystem::StartFireSale(float Duration)
{
    UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_Client) return;

    bIsFireSaleActive = true;

    for (const TObjectPtr<AMysteryBox>& Box : RegisteredBoxes)
    {
        if (IsValid(Box) && !Box->IsTemporaryFireSaleBox())
        {
            Box->SetFireSaleActive(true);
        }
    }

    TSubclassOf<AMysteryBox> ClassToSpawn = MysteryBoxClass;
    if (!ClassToSpawn && RegisteredBoxes.Num() > 0 && IsValid(RegisteredBoxes[0]))
    {
        ClassToSpawn = RegisteredBoxes[0]->GetClass();
    }

    if (!ClassToSpawn) return;

    for (const TObjectPtr<AMysteryBoxSpawnPoint>& Point : MysteryBoxSpawnPoints)
    {
        if (IsValid(Point) && !Point->IsOccupied())
        {
            AMysteryBox* TempBox = World->SpawnActorDeferred<AMysteryBox>(ClassToSpawn, Point->GetActorTransform());
            if (TempBox)
            {
                TempBox->SetIsTemporaryFireSaleBox(true);
                TempBox->AssignSpawnPoint(Point);
                TempBox->SetFireSaleActive(true);

                UGameplayStatics::FinishSpawningActor(TempBox, Point->GetActorTransform());

                Point->SetOccupied(true);
                TempFireSaleBoxes.Add(TempBox);
            }
        }
    }

    World->GetTimerManager().ClearTimer(TimerHandle_FireSale);
    World->GetTimerManager().SetTimer(TimerHandle_FireSale, this, &UZombieDirectorSubsystem::EndFireSale, Duration, false);
}

void UZombieDirectorSubsystem::EndFireSale()
{
    UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_Client) return;

    bIsFireSaleActive = false;

    for (const TObjectPtr<AMysteryBox>& Box : RegisteredBoxes)
    {
        if (IsValid(Box))
        {
            Box->SetFireSaleActive(false);
        }
    }

    for (const TObjectPtr<AMysteryBox>& TempBox : TempFireSaleBoxes)
    {
        if (IsValid(TempBox))
        {
            TempBox->HandleFireSaleEnd();
        }
    }

    TempFireSaleBoxes.Reset();
}