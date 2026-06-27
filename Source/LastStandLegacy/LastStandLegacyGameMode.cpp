// Fill out your copyright notice in the Description page of Project Settings.

#include "LastStandLegacyGameMode.h"
#include "Zombie.h"
#include "Hama.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "HamaPlayerState.h"
#include "ZombieSpawnPoint.h"
#include "LastStandLegacyGameState.h"

ALastStandLegacyGameMode::ALastStandLegacyGameMode()
{
}

void ALastStandLegacyGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);

    // Populate and shuffle abilities BEFORE any player (including the host) connects
    ActiveAbilities.Empty();
    ActiveAbilities.Add(EHamaAbilityType::BulletStorm);
    ActiveAbilities.Add(EHamaAbilityType::MedicalSupport);
    ActiveAbilities.Add(EHamaAbilityType::GhostMode);
    ActiveAbilities.Add(EHamaAbilityType::Blitz);

    for (int32 i = 0; i < ActiveAbilities.Num(); ++i)
    {
        int32 RandomIndex = FMath::RandRange(i, ActiveAbilities.Num() - 1);
        if (i != RandomIndex)
        {
            ActiveAbilities.Swap(i, RandomIndex);
        }
    }
}

void ALastStandLegacyGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    if (NewPlayer)
    {
        if (AHamaPlayerState* PS = NewPlayer->GetPlayerState<AHamaPlayerState>())
        {
            // ئەگەر یاریزانەکە هێشتا ڕۆڵی نییە و تواناش ماوە، پێی دەدەین
            if (PS->GetAssignedRole() == EHamaAbilityType::None && !ActiveAbilities.IsEmpty())
            {
                EHamaAbilityType AssignedAbility = ActiveAbilities.Pop();
                PS->SetAssignedRole(AssignedAbility);

                UE_LOG(LogTemp, Log, TEXT("Assigned permanent role to %s via PlayerState!"), *NewPlayer->GetName());
            }
            else if (ActiveAbilities.IsEmpty())
            {
                UE_LOG(LogTemp, Warning, TEXT("No abilities left for %s!"), *NewPlayer->GetName());
            }
        }
    }
}

void ALastStandLegacyGameMode::BeginPlay()
{
    Super::BeginPlay();

    SpawnPoints.Empty();

    for (TActorIterator<AZombieSpawnPoint> It(GetWorld()); It; ++It)
    {
        AZombieSpawnPoint* SpawnPoint = *It;
        SpawnPoints.Add(SpawnPoint);
    }

    if (SpawnPoints.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("No SpawnPoint found! Add 'ZombieSpawn' tag."));
    }

    CurrentRound = 1;
    DeadZombiesCount = 0;
    ActiveZombiesCount = 0;
    ZombiesSpawnedThisRound = 0;

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
            FString::Printf(TEXT("Round %d Started! Zombies this round: %d"), CurrentRound, ZombiesToKill));
    }

    GetWorldTimerManager().SetTimer(SpawnTimerHandle, this, &ALastStandLegacyGameMode::ProcessSpawning, 1.5f, true);
}

void ALastStandLegacyGameMode::HandleZombieDeath(AZombie* DeadZombie, AController* KillerController)
{
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, TEXT("Death Handler Triggered!"));

    if (KillerController)
    {
        APawn* KillerPawn = KillerController->GetPawn();
        if (KillerPawn)
        {
            UHamaAbilityComponent* AbilityComp = KillerPawn->FindComponentByClass<UHamaAbilityComponent>();
            if (AbilityComp)
            {
                float BasePowerReward = 15.0f / (1.0f + (CurrentRound * 0.15f));
                float PowerReward = FMath::Max(BasePowerReward, 2.0f);

                if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("Power Reward Calculated: %f"), PowerReward));

                AbilityComp->AddPower(PowerReward);
            }
            else
            {
                if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Error: AbilityComp NOT Found!"));
            }
        }
        else
        {
            if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Error: Killer Pawn is NULL!"));
        }
    }
    else
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, TEXT("KillerController is NULL (Trap Kill?)"));
    }
    DeadZombiesCount++;
    ActiveZombiesCount--;

    int32 ZombiesRemaining = ZombiesToKill - DeadZombiesCount;

    if (GEngine && ZombiesRemaining > 0)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Orange,
            FString::Printf(TEXT("Zombies Remaining: %d"), ZombiesRemaining));
    }

    if (CurrentPowerSpawn < MaxPowerSpawn)
    {
        float CurrentTime = GetWorld()->GetTimeSeconds();
        float CalculateTime = CurrentTime - CurrentPowerSpawnTime;

        if (CalculateTime >= PowerSpawnLimitTime)
        {
            float RandomChance = FMath::RandRange(0.0f, 100.0f);

            if (RandomChance <= PowerUpDropChance)
            {
                SpawnPowers(DeadZombie->GetActorLocation());
                CurrentPowerSpawnTime = CurrentTime;
            }
        }
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
        Zombie->OnZombieDeath.BindUObject(this, &ALastStandLegacyGameMode::HandleZombieDeath);
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

// لەسەرەوەی فایلەکە دڵنیابە کە ئینکلوودی ABasePowerUp کراوە
#include "BasePowerUp.h"

void ALastStandLegacyGameMode::SpawnPowers(FVector SpawnLocation)
{
    if (PowerUpClasses.IsEmpty())
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("ERROR: PowerUp array is empty in GameMode!"));
        return;
    }

    int32 RandomIndex = FMath::RandRange(0, PowerUpClasses.Num() - 1);

    TSubclassOf<ABasePowerUp> SelectedPowerUp = PowerUpClasses[RandomIndex];

    if (SelectedPowerUp)
    {
        FVector AdjustedLocation = SpawnLocation + FVector(0.0f, 0.0f, 40.0f);

        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        GetWorld()->SpawnActor<ABasePowerUp>(SelectedPowerUp, AdjustedLocation, FRotator::ZeroRotator, SpawnParams);

        CurrentPowerSpawn++;

        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, TEXT("Random PowerUp Spawned!"));
    }
}

void ALastStandLegacyGameMode::ActivateNuke()
{
    for (TActorIterator<AZombie> It(GetWorld()); It; ++It)
    {
        AZombie* Zombie = *It;
        if (Zombie && !Zombie->IsDead())
        {
            Zombie->Die(nullptr);
        }
    }

    bool bDoublePoints = false;
    if (ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>())
    {
        bDoublePoints = GS->bIsDoublePointsActive;
    }

    int32 NukeReward = bDoublePoints ? 800 : 400;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (APlayerController* PC = It->Get())
        {
            if (AHamaPlayerState* PS = PC->GetPlayerState<AHamaPlayerState>())
            {
                PS->AddPoints(NukeReward);
            }
        }
    }

    // لێرەدا دەتوانیت دەنگێکی گەورەی بۆمب یان ئیفێکتێکی شاشەی سپی (Flash) لەسەر کڵایەنتەکان لێبدەیت
}