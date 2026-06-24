// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HamaAbilityComponent.h"
#include "LastStandLegacyGameMode.generated.h"

class AZombie;
class AZombieSpawnPoint;

UCLASS()
class LASTSTANDLEGACY_API ALastStandLegacyGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ALastStandLegacyGameMode();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Settings")
    int32 CurrentRound = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Settings")
    int32 ZombiesSpawnLimit = 24;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Settings")
    int32 ZombiesToKill = 5;

    int32 DeadZombiesCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zombie Settings")
    int32 ActiveZombiesCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zombie Settings")
    int32 ZombiesSpawnedThisRound = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Settings")
    TSubclassOf<AZombie> ZombieClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Settings")
    TArray<AZombieSpawnPoint*> SpawnPoints;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Settings")
    float MinSafeDistance = 500.f;

    // لیستی تواناکان بۆ دابەشکردن بەسەر یاریزاناندا
    TArray<EHamaAbilityType> ActiveAbilities;

protected:
    // Core GameMode overrides for setup and connection handling
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void BeginPlay() override;

private:
    FTimerHandle SpawnTimerHandle;

    // Maps a player controller to their permanent assigned role
    UPROPERTY()
    TMap<AController*, EHamaAbilityType> AssignedPlayerRoles;

    void StartNextRound();
    void ProcessSpawning();
    AActor* PickWeightedSpawnPoint();

protected:
    UFUNCTION()
    void HandleZombieDeath(AZombie* DeadZombie, AController* KillerController);
};