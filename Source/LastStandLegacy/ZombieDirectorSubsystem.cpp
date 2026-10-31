#include "ZombieDirectorSubsystem.h"
#include "Zombie.h"
#include "AIController.h"
#include "MysteryBoxSpawnPoint.h" 
#include "Navigation/PathFollowingComponent.h"
#include "Engine/World.h"
#include "MysteryBox.h"
#include "Kismet/GameplayStatics.h"
#include "Hama.h"

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
    CachedPlayerLocations.Reset();

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
            CachedPlayerLocations.Add({ Player, Player->GetActorLocation() });
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

        CachedPlayerLocations.RemoveAllSwap(
            [Player](const FCachedTargetInfo& Info)
            {
                return !Info.PlayerPawn.IsValid() || Info.PlayerPawn.Get() == Player;
            },
            EAllowShrinking::No
        );
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

    const FTransform PoolSpawnTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, -300.f));

    for (int32 i = 0; i < PoolSize; ++i)
    {
        AZombie* NewZombie = World->SpawnActorDeferred<AZombie>(
            PooledZombieClass,
            PoolSpawnTransform,
            nullptr,
            nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn
        );

        if (NewZombie)
        {
            NewZombie->SetActorHiddenInGame(true);
            UGameplayStatics::FinishSpawningActor(NewZombie, PoolSpawnTransform);
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

        CachedPlayerLocations.Reset();

        for (int32 Idx = ValidTargetPlayers.Num() - 1; Idx >= 0; --Idx)
        {
            if (ValidTargetPlayers[Idx].IsValid())
            {
                APawn* P = ValidTargetPlayers[Idx].Get();
                if (IsValid(P))
                {
                    CachedPlayerLocations.Add({ P, P->GetActorLocation() });
                }
            }
            else
            {
                ValidTargetPlayers.RemoveAtSwap(Idx, 1, EAllowShrinking::No);
            }
        }

        for (int32 Idx = ActiveZombies.Num() - 1; Idx >= 0; --Idx)
        {
            AZombie* Z = ActiveZombies[Idx].Get();
            if (!IsValid(Z) || Z->IsDead())
            {
                ActiveZombies.RemoveAtSwap(Idx, 1, EAllowShrinking::No);
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
        bool bIsTargetAttractor = false;

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
                        bIsTargetAttractor = true;
                    }
                }
            }
        }

        // =========================================================
        // B. دیاریکردنی یاریزان و وەرگرتنی شوێنی ڕاستەقینەی ئێستا
        // =========================================================
        if (!FinalTargetActor)
        {
            bool bNeedsSearch = (CurrentTime >= Zombie->NextTargetSearchTime || !IsValid(Zombie->CurrentTarget));

            if (!bNeedsSearch && Zombie->CurrentTarget)
            {
                for (const FCachedTargetInfo& TargetInfo : CachedPlayerLocations)
                {
                    if (TargetInfo.PlayerPawn.Get() == Zombie->CurrentTarget)
                    {
                        FinalTargetActor = TargetInfo.PlayerPawn.Get();
                        FinalTargetLoc = TargetInfo.Location;
                        ClosestDistanceSq = FVector::DistSquared(ZombieLoc, FinalTargetLoc);
                        break;
                    }
                }

                if (!FinalTargetActor)
                {
                    bNeedsSearch = true;
                }
            }

            if (bNeedsSearch)
            {
                for (const FCachedTargetInfo& TargetInfo : CachedPlayerLocations)
                {
                    APawn* Candidate = TargetInfo.PlayerPawn.Get();
                    if (!IsValid(Candidate)) continue;

                    const float DistSq = FVector::DistSquared(ZombieLoc, TargetInfo.Location);
                    if (DistSq < ClosestDistanceSq)
                    {
                        ClosestDistanceSq = DistSq;
                        FinalTargetActor = Candidate;
                        FinalTargetLoc = TargetInfo.Location;
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
        // C. Dynamic Network LOD & AI Repathing + Smart Unstuck
        // =========================================================
        AAIController* AICon = Zombie->CachedAIController;
        if (!AICon) continue;

        if (FinalTargetActor)
        {
            // فۆکەس خستنە سەر ئامانج کاتێک نزیکە (تەنانەت لە کاتی هێرشکردنیشدا کار دەکات تا زۆمبییەکە بەردەوام ڕووی بسووڕێت)
            if (ClosestDistanceSq < 400000.f)
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

            // ١. ئەگەر ئامانجەکە مەیموون بوو (Attractor): هەرگیز هێرش مەکە، تەنها لە دەوری کۆببەرەوە و بوەستە!
            if (bIsTargetAttractor)
            {
                const float AttractorStopDistSq = FMath::Square(90.f);
                if (ClosestDistanceSq <= AttractorStopDistSq)
                {
                    Zombie->CurrentTarget = FinalTargetActor;
                    Zombie->LastTargetLocation = FinalTargetLoc;
                    if (AICon->GetMoveStatus() != EPathFollowingStatus::Idle)
                    {
                        AICon->StopMovement();
                    }
                    continue;
                }
            }
            // ٢. ئەگەر ئامانجەکە یاریزان بوو: کاتێک دەگاتە مەودای لێدان، هێرش بکە
            else if (ClosestDistanceSq <= Zombie->AttackDistanceSq)
            {
                Zombie->CurrentTarget = FinalTargetActor;
                Zombie->TryAttack();
                continue;
            }

            if (Zombie->IsAttacking())
            {
                continue;
            }

            const uint8 Tier = (ClosestDistanceSq < 1440000.f) ? 0 : (ClosestDistanceSq < 6250000.f ? 1 : 2);
            Zombie->ApplyServerLOD(Tier);

            const float MinRepathInterval =
                (ClosestDistanceSq < 360000.f) ? 0.15f :
                (ClosestDistanceSq < 2250000.f) ? 0.35f : 0.8f;

            float DynamicMovedThresholdSq = 180.f * 180.f;
            if (ClosestDistanceSq < 360000.f)
            {
                DynamicMovedThresholdSq = 90.f * 90.f;
            }

            const float TargetMovedDistSq = FVector::DistSquared(Zombie->LastTargetLocation, FinalTargetLoc);
            const EPathFollowingStatus::Type MoveStatus = AICon->GetMoveStatus();
            const bool bIsAIIdle = (MoveStatus == EPathFollowingStatus::Idle);
            const bool bTargetChanged = (Zombie->CurrentTarget != FinalTargetActor);
            const bool bThrottleOpen = (CurrentTime >= Zombie->NextRepathAllowedTime);

            const float CurrentSpeedSq = Zombie->GetVelocity().SizeSquared2D();
            const bool bIsBlocked = (MoveStatus == EPathFollowingStatus::Moving) && (CurrentSpeedSq < 900.f);

            // ئەگەر زۆمبییەکە زۆر نزیک بوو لە مەیموونەکە و لەبەر قەرەباڵغی زۆمبییەکانی تر وەستا، با لە شوێنی خۆی بوەستێت
            if (bIsTargetAttractor && bIsBlocked && ClosestDistanceSq <= FMath::Square(220.f))
            {
                Zombie->CurrentTarget = FinalTargetActor;
                Zombie->LastTargetLocation = FinalTargetLoc;
                AICon->StopMovement();
                continue;
            }

            const bool bShouldRepath = bTargetChanged ||
                (bThrottleOpen && (bIsBlocked ||
                    TargetMovedDistSq > DynamicMovedThresholdSq ||
                    (bIsAIIdle && CurrentTime >= Zombie->NextForceRepathTime)));

            if (bShouldRepath)
            {
                if (CurrentRepathCount >= MaxRepathRequestsThisFrame)
                {
                    break;
                }

                FVector MoveGoalLoc = FinalTargetLoc;

                if (bIsBlocked && !bTargetChanged)
                {
                    const FVector DirToTarget = (FinalTargetLoc - ZombieLoc).GetSafeNormal2D();
                    const FVector RightDir(-DirToTarget.Y, DirToTarget.X, 0.f);
                    const float SideSign = FMath::RandBool() ? 1.f : -1.f;

                    const FVector CandidateA = ZombieLoc + (DirToTarget * 110.f) + (RightDir * (160.f * SideSign));
                    const FVector CandidateB = ZombieLoc + (DirToTarget * 110.f) - (RightDir * (160.f * SideSign));

                    FHitResult Hit;
                    FCollisionQueryParams Params(SCENE_QUERY_STAT(ZombieUnstuck), false, Zombie);
                    const FVector EyeOffset(0.f, 0.f, 45.f);

                    if (World->LineTraceSingleByChannel(Hit, ZombieLoc + EyeOffset, CandidateA + EyeOffset, ECC_Pawn, Params))
                    {
                        MoveGoalLoc = CandidateB;
                    }
                    else
                    {
                        MoveGoalLoc = CandidateA;
                    }

                    Zombie->NextRepathAllowedTime = CurrentTime + 0.4f;
                }
                else
                {
                    const float NextDelay = bIsAIIdle ? FMath::Max(MinRepathInterval, 0.25f) : MinRepathInterval;
                    Zombie->NextRepathAllowedTime = CurrentTime + NextDelay;
                }

                Zombie->CurrentTarget = FinalTargetActor;
                Zombie->LastTargetLocation = MoveGoalLoc;

                if (bIsAIIdle)
                {
                    Zombie->NextForceRepathTime = CurrentTime + 0.2f;
                }

                FAIMoveRequest MoveReq(MoveGoalLoc);
                const float AcceptRadius = bIsBlocked ? 25.f : (bIsTargetAttractor ? 45.f : (Zombie->AttackDistance * 0.5f));
                MoveReq.SetAcceptanceRadius(AcceptRadius);
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