#include "LastStandLegacyGameMode.h"
#include "Zombie.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

ALastStandLegacyGameMode::ALastStandLegacyGameMode()
{
}

void ALastStandLegacyGameMode::BeginPlay()
{
    Super::BeginPlay();
    SpawnZombiesForRound();
}

// ئەمە فەنگسنی نوێیەکە کە وەڵامی مردنی زۆمبی دەداتەوە
void ALastStandLegacyGameMode::HandleZombieDeath(AZombie* DeadZombie)
{
    DeadZombiesCount++;

    if (DeadZombiesCount >= ZombiesToKill)
    {
        StartNextRound();
    }
}

void ALastStandLegacyGameMode::StartNextRound()
{
    CurrentRound++;
    DeadZombiesCount = 0;
    ZombiesToKill += 5;

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
            FString::Printf(TEXT("Round %d Started"), CurrentRound));
    }

    for (TActorIterator<AZombie> It(GetWorld()); It; ++It)
    {
        if (*It)
        {
            (*It)->SetStatsForRound(CurrentRound);
        }
    }

    SpawnZombiesForRound();
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

void ALastStandLegacyGameMode::SpawnZombiesForRound()
{
    if (!ZombieClass || SpawnPoints.IsEmpty()) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    for (int32 i = 0; i < ZombiesToKill; i++)
    {
        AActor* SpawnPoint = PickWeightedSpawnPoint();
        if (!SpawnPoint) continue;

        AZombie* Zombie = GetWorld()->SpawnActor<AZombie>(
            ZombieClass, SpawnPoint->GetActorLocation(), SpawnPoint->GetActorRotation(), SpawnParams);

        if (Zombie)
        {
            Zombie->SetStatsForRound(CurrentRound);

            // ئەم بەشە زۆر گرنگە: لێرەدا زۆمبییەکە گرێ دەدەین بە HandleZombieDeath
            Zombie->OnZombieDeath.AddDynamic(this, &ALastStandLegacyGameMode::HandleZombieDeath);
        }
    }
}