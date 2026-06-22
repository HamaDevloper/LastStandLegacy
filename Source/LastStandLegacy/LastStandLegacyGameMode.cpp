#include "LastStandLegacyGameMode.h"
#include "Zombie.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

ALastStandLegacyGameMode::ALastStandLegacyGameMode()
{
}

void ALastStandLegacyGameMode::BeginPlay()
{
    Super::BeginPlay();

    SpawnPoints.Empty();

    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* Actor = *It;
        if (Actor && Actor->ActorHasTag(FName("ZombieSpawn")))
        {
            SpawnPoints.Add(Actor);
        }
    }

    if (SpawnPoints.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("No SpawnPoint found! Add 'ZombieSpawn' tag."));
    }

    CurrentRound = 1;
    DeadZombiesCount = 0;
    ActiveZombiesCount = 0;
    ZombiesSpawnedThisRound = 0;

    // نامەی دەستپێکی یاری (ڕاوندی یەکەم) لەگەڵ ژمارەی زۆمبییەکان
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
            FString::Printf(TEXT("Round %d Started! Zombies this round: %d"), CurrentRound, ZombiesToKill));
    }

    GetWorldTimerManager().SetTimer(SpawnTimerHandle, this, &ALastStandLegacyGameMode::ProcessSpawning, 1.5f, true);
}

void ALastStandLegacyGameMode::HandleZombieDeath(AZombie* DeadZombie)
{
    DeadZombiesCount++;
    ActiveZombiesCount--;

    // هەژمارکردنی ئەو زۆمبیانەی کە ماون بکوژرێن
    int32 ZombiesRemaining = ZombiesToKill - DeadZombiesCount;

    // پرینتکردنی ژمارەی زۆمبییە ماوەکان لەسەر شاشە (تەنها ئەگەر گەورەتر بوو لە سفڕ)
    if (GEngine && ZombiesRemaining > 0)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Orange,
            FString::Printf(TEXT("Zombies Remaining: %d"), ZombiesRemaining));
    }

    if (DeadZombiesCount >= ZombiesToKill)
    {
        StartNextRound();
    }
}

void ALastStandLegacyGameMode::StartNextRound()
{
    CurrentRound++;

    DeadZombiesCount = 0;
    ZombiesSpawnedThisRound = 0;
    ActiveZombiesCount = 0;

    ZombiesToKill += 5;

    // نامەی دەستپێکردنی ڕاوندەکانی تر لەگەڵ ژمارەی زۆمبییەکان
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
            FString::Printf(TEXT("Round %d Started! Zombies this round: %d"), CurrentRound, ZombiesToKill));
    }

    for (TActorIterator<AZombie> It(GetWorld()); It; ++It)
    {
        if (*It)
        {
            (*It)->SetStatsForRound(CurrentRound);
        }
    }

    GetWorldTimerManager().SetTimer(SpawnTimerHandle, this, &ALastStandLegacyGameMode::ProcessSpawning, 1.5f, true);
}

void ALastStandLegacyGameMode::ProcessSpawning()
{
    if (ZombiesSpawnedThisRound >= ZombiesToKill)
    {
        GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
        return;
    }

    if (ActiveZombiesCount >= ZombiesSpawnLimit)
    {
        return;
    }

    if (!ZombieClass || SpawnPoints.IsEmpty()) return;

    AActor* SpawnPoint = PickWeightedSpawnPoint();
    if (!SpawnPoint) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    AZombie* Zombie = GetWorld()->SpawnActor<AZombie>(
        ZombieClass, SpawnPoint->GetActorLocation(), SpawnPoint->GetActorRotation(), SpawnParams);

    if (Zombie)
    {
        ActiveZombiesCount++;
        ZombiesSpawnedThisRound++;

        Zombie->SetStatsForRound(CurrentRound);
        Zombie->OnZombieDeath.AddUObject(this, &ALastStandLegacyGameMode::HandleZombieDeath);
    }
}

AActor* ALastStandLegacyGameMode::PickWeightedSpawnPoint()
{
    if (SpawnPoints.IsEmpty()) return nullptr;

    TArray<APawn*> Players;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (APlayerController* PC = It->Get())
        {
            if (APawn* Pawn = PC->GetPawn())
            {
                Players.Add(Pawn);
            }
        }
    }

    struct FScoredPoint { AActor* Point; float Weight; };
    TArray<FScoredPoint> ScoredPoints;
    float TotalWeight = 0.f;
    const float MinSafeDistSq = FMath::Square(MinSafeDistance);

    for (AActor* SP : SpawnPoints)
    {
        if (!SP) continue;
        float ClosestDistSq = TNumericLimits<float>::Max();

        for (APawn* Player : Players)
        {
            float DistSq = FVector::DistSquared(SP->GetActorLocation(), Player->GetActorLocation());
            ClosestDistSq = FMath::Min(ClosestDistSq, DistSq);
        }

        if (!Players.IsEmpty() && ClosestDistSq < MinSafeDistSq) continue;

        float Weight = FMath::Sqrt(ClosestDistSq) + 100.f;
        ScoredPoints.Add({ SP, Weight });
        TotalWeight += Weight;
    }

    if (ScoredPoints.IsEmpty())
    {
        return SpawnPoints[FMath::RandRange(0, SpawnPoints.Num() - 1)];
    }

    float RandomValue = FMath::FRandRange(0.f, TotalWeight);
    float CurrentWeight = 0.f;

    for (const FScoredPoint& Entry : ScoredPoints)
    {
        CurrentWeight += Entry.Weight;
        if (RandomValue <= CurrentWeight) return Entry.Point;
    }

    return ScoredPoints.Last().Point;
}