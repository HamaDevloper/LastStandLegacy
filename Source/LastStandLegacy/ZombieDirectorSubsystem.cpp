#include "ZombieDirectorSubsystem.h"
#include "Zombie.h"
#include "AIController.h"
#include "MysteryBoxSpawnPoint.h" 
#include "Navigation/PathFollowingComponent.h"
#include "Engine/World.h"
#include "MysteryBox.h"
#include "Kismet/GameplayStatics.h"
#include "Hama.h"
#include "ZombieSpawnPoint.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

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
    InactiveZombiePool.Empty();
    ActivePlayers.Empty();
    ValidTargetPlayers.Empty();
    ActiveAttractors.Empty();
    MysteryBoxSpawnPoints.Empty();
    ReusableMysteryBoxPoints.Empty();
    RegisteredBoxes.Empty();
    TempFireSaleBoxes.Empty();
    ZombieSpawnPoints.Empty();
    UnlockedZones.Empty();
    CachedPlayerLocations.Reset();

    LastUsedSpawnPoint.Reset();
    ConsecutiveSpawnsOnLastPoint = 0;
    CurrentZombieIndex = 0;
    RoundRobinPlayerIndex = 0;

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

void UZombieDirectorSubsystem::RegisterZombieSpawnPoints(const TArray<AZombieSpawnPoint*>& InSpawnPoints)
{
    ZombieSpawnPoints.Reset(InSpawnPoints.Num());
    UnlockedZones.Reset();
    UnlockedZones.Add(0);

    for (AZombieSpawnPoint* SP : InSpawnPoints)
    {
        if (IsValid(SP))
        {
            SP->SetSpawnActive(UnlockedZones.Contains(SP->ZoneID));
            ZombieSpawnPoints.Add(SP);
        }
    }
}

void UZombieDirectorSubsystem::UnlockZone(int32 ZoneIDToUnlock)
{
    if (UnlockedZones.Contains(ZoneIDToUnlock)) return;

    UnlockedZones.Add(ZoneIDToUnlock);

    for (AZombieSpawnPoint* SP : ZombieSpawnPoints)
    {
        if (IsValid(SP) && SP->ZoneID == ZoneIDToUnlock)
        {
            SP->SetSpawnActive(true);
        }
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

    // چاککردنی Opt 3: Preload کردنی هەموو مێشەکانی زۆمبی لە سەرەتاوە تا کاتی سپاون دیار نەبن
    if (const AZombie* DefaultZombie = InZombieClass->GetDefaultObject<AZombie>())
    {
        FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
        for (const TSoftObjectPtr<USkeletalMesh>& SoftMesh : DefaultZombie->MeshToSelect)
        {
            if (!SoftMesh.IsNull())
            {
                Streamable.RequestAsyncLoad(SoftMesh.ToSoftObjectPath());
            }
        }
    }

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
    if (IsValid(Zombie))
    {
        InactiveZombiePool.Add(Zombie);
    }
}

void UZombieDirectorSubsystem::ExecuteElectricCherryBlast(AActor* InstigatorPawn, AController* InstigatorController, const FVector& Origin, float Radius, float FixedDamage)
{
    if (!InstigatorPawn || ActiveZombies.IsEmpty()) return;

    UWorld* World = GetWorld();
    if (!World) return;

#if ENABLE_DRAW_DEBUG
    DrawDebugSphere(
        World,
        Origin,
        Radius,
        32,
        FColor::Cyan,
        false,
        2.5f,
        0,
        2.5f
    );
#endif

    const float RadiusSq = FMath::Square(Radius);

    for (int32 i = ActiveZombies.Num() - 1; i >= 0; --i)
    {
        AZombie* Zombie = ActiveZombies[i].Get();
        if (!IsValid(Zombie) || Zombie->IsDead()) continue;

        const float DistSq = FVector::DistSquared(Origin, Zombie->GetActorLocation());
        if (DistSq <= RadiusSq)
        {
#if ENABLE_DRAW_DEBUG
            DrawDebugLine(
                World,
                Origin,
                Zombie->GetActorLocation(),
                FColor::Yellow,
                false,
                1.5f,
                0,
                1.5f
            );
#endif
            // ١. لێدانی دەمەیجی جێگیر و بەرز بە هەموو زۆمبییەکانی ناو بازنەکە وەک یەک
            UGameplayStatics::ApplyDamage(Zombie, FixedDamage, InstigatorController, InstigatorPawn, UDamageType::StaticClass());

            // ٢. ئەگەر زۆمبییەکە لە ڕاوندی بەرز بوو و بە دەمەیجەکە نەمرد، ستەنی بکە تا ئەەنیمەیشنەکەی تەواو دەبێت
            if (!Zombie->IsDead())
            {
                Zombie->ApplyElectricStun();
            }
        }
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

    const int32 MaxRepathRequestsThisFrame = FMath::Clamp(ZombieCount / 4, 4, 8);
    int32 CurrentRepathCount = 0;
    bool bRelocatedThisFrame = false; // چاککردنی Opt 2: تەنها ١ گواستنەوە لە هەر فڕەیمێکدا

    int32 i = CurrentZombieIndex;
    for (; i < ZombieCount; ++i)
    {
        if ((FPlatformTime::Seconds() - StartTime) > DynamicTimeBudget)
        {
            break;
        }

        AZombie* Zombie = ActiveZombies[i].Get();
        if (!IsValid(Zombie) || Zombie->IsDead()) continue;

        if (Zombie->IsStunned(CurrentTime) || Zombie->bIsAttacking)
        {
            continue;
        }

        const FVector ZombieLoc = Zombie->GetActorLocation();

        // چاککردنی Bug 4: دۆزینەوەی مەودای نزیکترین یاریزان بە سەربەخۆیی بۆ LOD و Recycle
        float NearestPlayerDistSq = UE_BIG_NUMBER;
        for (const FCachedTargetInfo& Info : CachedPlayerLocations)
        {
            NearestPlayerDistSq = FMath::Min(NearestPlayerDistSq, FVector::DistSquared(ZombieLoc, Info.Location));
        }

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
        // B. دیاریکردنی یاریزان
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
                    float NextInterval = 0.25f;
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
            // ١. گواستنەوەی زۆمبی زۆر دوور بەپێی NearestPlayerDistSq و سنووری ١ جار لە فڕەیمێکدا
            constexpr float RecycleDistSq = 3500.f * 3500.f;
            if (!bRelocatedThisFrame && !bIsTargetAttractor && NearestPlayerDistSq > RecycleDistSq && CurrentTime >= Zombie->NextRecycleCheckTime)
            {
                Zombie->NextRecycleCheckTime = CurrentTime + 1.5f;

                if ((CurrentTime - Zombie->LastSpawnOrRelocateTime) > 6.0f &&
                    !IsLocationVisibleToAnyPlayer(ZombieLoc, 5000.f * 5000.f))
                {
                    if (AActor* NewSP = PickWeightedSpawnPoint(600.f, 2800.f, true))
                    {
                        Zombie->RelocateTo(NewSP->GetActorLocation(), NewSP->GetActorRotation());
                        bRelocatedThisFrame = true;
                        continue;
                    }
                }
            }

            const EPathFollowingStatus::Type MoveStatus = AICon->GetMoveStatus();
            const float CurrentSpeedSq = Zombie->GetVelocity().SizeSquared2D();
            const bool bIsBlocked = (MoveStatus == EPathFollowingStatus::Moving) && (CurrentSpeedSq < 900.f);
            const bool bIsAIIdle = (MoveStatus == EPathFollowingStatus::Idle);

            // ٢. ڕێکخستنی سوڕانەوە
            const float DirectFaceDistSq = FMath::Max(Zombie->AttackTriggerDistanceSq * 2.25f, 122500.f);
            const bool bUseDirectFocusRotation = !bIsBlocked && (ClosestDistanceSq <= DirectFaceDistSq || bIsAIIdle || CurrentSpeedSq < 400.f);

            if (ClosestDistanceSq < 640000.f && !bIsBlocked)
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

            if (UCharacterMovementComponent* MoveComp = Zombie->GetCharacterMovement())
            {
                MoveComp->bOrientRotationToMovement = !bUseDirectFocusRotation;
                MoveComp->bUseControllerDesiredRotation = bUseDirectFocusRotation;
            }

            // ٣. ئەگەر ئامانجەکە مەیموون بوو یان گەیشتە مەودای هێرشکردن
            if (bIsTargetAttractor)
            {
                const float AttractorStopDistSq = FMath::Square(90.f);
                if (ClosestDistanceSq <= AttractorStopDistSq)
                {
                    Zombie->CurrentTarget = FinalTargetActor;
                    Zombie->LastTargetLocation = FinalTargetLoc;
                    if (!bIsAIIdle)
                    {
                        AICon->StopMovement();
                    }
                    continue;
                }
            }
            else if (ClosestDistanceSq <= Zombie->AttackTriggerDistanceSq)
            {
                Zombie->CurrentTarget = FinalTargetActor;
                Zombie->LastTargetLocation = FinalTargetLoc;
                if (!bIsAIIdle)
                {
                    AICon->StopMovement();
                }
                Zombie->TryAttack();
                continue;
            }

            // چاککردنی Bug 4: دیاریکردنی Tier بەپێی نزیکترین یاریزان نەک مەیموونەکە
            const uint8 Tier = (NearestPlayerDistSq < 1440000.f) ? 0 : (NearestPlayerDistSq < 6250000.f ? 1 : 2);
            Zombie->ApplyServerLOD(Tier);

            const float MinRepathInterval =
                (ClosestDistanceSq < 360000.f) ? 0.25f :
                (ClosestDistanceSq < 2250000.f) ? 0.45f : 0.85f;

            float DynamicMovedThresholdSq = 140.f * 140.f;
            if (ClosestDistanceSq < 360000.f)
            {
                DynamicMovedThresholdSq = 55.f * 55.f;
            }
            else if (ClosestDistanceSq < 1440000.f)
            {
                DynamicMovedThresholdSq = 90.f * 90.f;
            }

            const float TargetMovedDistSq = FVector::DistSquared(Zombie->LastTargetLocation, FinalTargetLoc);
            const bool bTargetChanged = (Zombie->CurrentTarget != FinalTargetActor);
            const bool bThrottleOpen = (CurrentTime >= Zombie->NextRepathAllowedTime);

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

                if (bIsBlocked && !bTargetChanged && ClosestDistanceSq > Zombie->AttackTriggerDistanceSq)
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

                    Zombie->NextRepathAllowedTime = CurrentTime + 0.55f;
                }
                else
                {
                    const float NextDelay = bIsAIIdle ? FMath::Max(MinRepathInterval, 0.3f) : MinRepathInterval;
                    Zombie->NextRepathAllowedTime = CurrentTime + NextDelay;
                }

                Zombie->CurrentTarget = FinalTargetActor;
                Zombie->LastTargetLocation = FinalTargetLoc;

                if (bIsAIIdle)
                {
                    Zombie->NextForceRepathTime = CurrentTime + 0.35f;
                }

                FAIMoveRequest MoveReq(MoveGoalLoc);
                const float AcceptRadius = bIsBlocked ? 25.f : (bIsTargetAttractor ? 45.f : 55.f);
                MoveReq.SetAcceptanceRadius(AcceptRadius);
                MoveReq.SetUsePathfinding(true); // چاککردنی Bug 5: هەمیشە لەسەر NavMesh بمێنێتەوە تا لە دیوار گیر نەخوات
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

                if (UCharacterMovementComponent* MoveComp = Zombie->GetCharacterMovement())
                {
                    MoveComp->bOrientRotationToMovement = true;
                    MoveComp->bUseControllerDesiredRotation = false;
                }
            }
        }
    }

    CurrentZombieIndex = (i >= ZombieCount) ? 0 : i;
}

TStatId UZombieDirectorSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UZombieDirectorSubsystem, STATGROUP_Tickables);
}

bool UZombieDirectorSubsystem::IsLocationVisibleToAnyPlayer(const FVector& WorldLoc, float MaxCheckDistSq) const
{
    UWorld* World = GetWorld();
    if (!World) return false;

    const FVector TargetCheckLoc = WorldLoc + FVector(0.f, 0.f, 60.f);

    for (const FCachedTargetInfo& Info : CachedPlayerLocations)
    {
        APawn* PlayerPawn = Info.PlayerPawn.Get();
        if (!IsValid(PlayerPawn)) continue;

        const float DistSq = FVector::DistSquared(Info.Location, WorldLoc);
        if (DistSq > MaxCheckDistSq) continue;

        FVector ViewLoc = Info.Location + FVector(0.f, 0.f, 64.f);
        FRotator ViewRot = PlayerPawn->GetActorRotation();

        if (AController* Con = PlayerPawn->GetController())
        {
            Con->GetPlayerViewPoint(ViewLoc, ViewRot);
        }

        const FVector DirToTarget = (TargetCheckLoc - ViewLoc).GetSafeNormal();
        const float Dot = FVector::DotProduct(ViewRot.Vector(), DirToTarget);

        if (Dot > 0.30f)
        {
            FCollisionQueryParams LosParams(SCENE_QUERY_STAT(ZombieVisibilityCheck), false, PlayerPawn);
            if (!World->LineTraceTestByChannel(ViewLoc, TargetCheckLoc, ECC_Visibility, LosParams))
            {
                return true;
            }
        }
    }

    return false;
}

AZombieSpawnPoint* UZombieDirectorSubsystem::PickWeightedSpawnPoint(float MinSafeDist, float MaxSpawnDist, bool bIsRelocation)
{
    if (ZombieSpawnPoints.IsEmpty()) return nullptr;

    if (CachedPlayerLocations.IsEmpty())
    {
        for (const TWeakObjectPtr<APawn>& WeakPawn : ActivePlayers)
        {
            if (APawn* P = WeakPawn.Get())
            {
                if (IsValid(P))
                {
                    CachedPlayerLocations.Add({ P, P->GetActorLocation() });
                }
            }
        }
    }

    // لامبدا (Lambda) بۆ تۆمارکردنی خاڵی هەڵبژێردراو و ڕیسێتکردنەوەی خاڵی پێشوو
    auto RecordAndReturnSpawn = [this, bIsRelocation](AZombieSpawnPoint* ChosenPoint) -> AZombieSpawnPoint*
        {
            if (ChosenPoint && !bIsRelocation)
            {
                if (LastUsedSpawnPoint.Get() == ChosenPoint)
                {
                    ConsecutiveSpawnsOnLastPoint++;
                }
                else
                {
                    LastUsedSpawnPoint = ChosenPoint;
                    ConsecutiveSpawnsOnLastPoint = 1;
                }
            }
            return ChosenPoint;
        };

    if (CachedPlayerLocations.IsEmpty())
    {
        for (AZombieSpawnPoint* SP : ZombieSpawnPoints)
        {
            if (IsValid(SP) && SP->IsSpawnActive())
            {
                return RecordAndReturnSpawn(SP);
            }
        }
        return nullptr;
    }

    RoundRobinPlayerIndex = (RoundRobinPlayerIndex + 1) % CachedPlayerLocations.Num();
    const FVector TargetLocation = CachedPlayerLocations[RoundRobinPlayerIndex].Location;

    struct FScoredPoint { AZombieSpawnPoint* Point; float Weight; };
    TArray<FScoredPoint, TInlineAllocator<16>> ScoredPoints;
    float TotalWeight = 0.f;

    const float MinSafeDistSq = FMath::Square(MinSafeDist);
    const float MaxSpawnDistSq = FMath::Square(MaxSpawnDist);

    for (AZombieSpawnPoint* SP : ZombieSpawnPoints)
    {
        if (!IsValid(SP) || !SP->IsSpawnActive()) continue;

        // ئەگەر ئەم خاڵە گەیشتبێتە سنووری لەسەریەک (بۆ نموونە ٢ جار)، باز بدە بۆ خاڵێکی تر
        const bool bIsOverusedPoint = (SP == LastUsedSpawnPoint.Get() && ConsecutiveSpawnsOnLastPoint >= MaxConsecutiveSpawnsPerPoint);
        if (bIsOverusedPoint) continue;

        const FVector SpLoc = SP->GetActorLocation();

        bool bTooCloseToAnyPlayer = false;
        for (const FCachedTargetInfo& PInfo : CachedPlayerLocations)
        {
            if (FVector::DistSquared(SpLoc, PInfo.Location) < MinSafeDistSq)
            {
                bTooCloseToAnyPlayer = true;
                break;
            }
        }
        if (bTooCloseToAnyPlayer) continue;

        const float DistSqToTarget = FVector::DistSquared(SpLoc, TargetLocation);
        if (DistSqToTarget > MaxSpawnDistSq) continue;

        if (bIsRelocation && IsLocationVisibleToAnyPlayer(SpLoc, FMath::Square(1800.f)))
        {
            continue;
        }

        const float ActualDist = FMath::Sqrt(DistSqToTarget);
        const float Weight = (MaxSpawnDist - ActualDist) + 200.0f;

        ScoredPoints.Add({ SP, Weight });
        TotalWeight += Weight;
    }

    // Fallback: ئەگەر هیچ خاڵێک لە مەودای نموونەییدا نەبوو یان خاڵە نزیکەکە قفڵ ببوو
    if (ScoredPoints.IsEmpty())
    {
        AZombieSpawnPoint* BestSafeFallback = nullptr;
        float BestSafeDistSq = TNumericLimits<float>::Max();

        // ئەگەر لە تەواوی ناوچەکەدا تەنها ١ خاڵی سەلامەت هەبوو، وەک دواچارەسەر هەڵیدەگرین
        AZombieSpawnPoint* OverusedSafeFallback = nullptr;
        float OverusedSafeDistSq = TNumericLimits<float>::Max();

        AZombieSpawnPoint* FurthestEmergencyFallback = nullptr;
        float MaxMinDistSqToPlayers = -1.f;

        for (AZombieSpawnPoint* SP : ZombieSpawnPoints)
        {
            if (!IsValid(SP) || !SP->IsSpawnActive()) continue;

            const FVector SpLoc = SP->GetActorLocation();

            float ClosestPlayerDistSq = TNumericLimits<float>::Max();
            for (const FCachedTargetInfo& PInfo : CachedPlayerLocations)
            {
                const float D = FVector::DistSquared(SpLoc, PInfo.Location);
                if (D < ClosestPlayerDistSq) ClosestPlayerDistSq = D;
            }

            if (ClosestPlayerDistSq > MaxMinDistSqToPlayers)
            {
                MaxMinDistSqToPlayers = ClosestPlayerDistSq;
                FurthestEmergencyFallback = SP;
            }

            if (ClosestPlayerDistSq < MinSafeDistSq) continue;

            if (bIsRelocation && IsLocationVisibleToAnyPlayer(SpLoc, FMath::Square(1500.f)))
            {
                continue;
            }

            const float DistSqToTarget = FVector::DistSquared(SpLoc, TargetLocation);
            const bool bIsOverusedPoint = (SP == LastUsedSpawnPoint.Get() && ConsecutiveSpawnsOnLastPoint >= MaxConsecutiveSpawnsPerPoint);

            if (!bIsOverusedPoint)
            {
                if (DistSqToTarget < BestSafeDistSq)
                {
                    BestSafeDistSq = DistSqToTarget;
                    BestSafeFallback = SP;
                }
            }
            else if (DistSqToTarget < OverusedSafeDistSq)
            {
                OverusedSafeDistSq = DistSqToTarget;
                OverusedSafeFallback = SP;
            }
        }

        // ١. یەکەم جار نزیکترین خاڵی تر هەڵدەبژێرێت کە دووبارە نەبووبێتەوە
        if (BestSafeFallback)
        {
            return RecordAndReturnSpawn(BestSafeFallback);
        }

        // ٢. ئەگەر لە مەپەکەدا تەنها هەر ئەو خاڵە سەلامەتە هەبوو، ناچار بەکاریدەهێنێتەوە
        if (OverusedSafeFallback)
        {
            return RecordAndReturnSpawn(OverusedSafeFallback);
        }

        return bIsRelocation ? nullptr : RecordAndReturnSpawn(FurthestEmergencyFallback);
    }

    const float RandomValue = FMath::FRandRange(0.f, TotalWeight);
    float CurrentWeight = 0.f;

    for (const FScoredPoint& Entry : ScoredPoints)
    {
        CurrentWeight += Entry.Weight;
        if (RandomValue <= CurrentWeight)
        {
            return RecordAndReturnSpawn(Entry.Point);
        }
    }

    return RecordAndReturnSpawn(ScoredPoints.Last().Point);
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