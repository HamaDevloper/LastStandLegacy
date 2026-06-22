// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HamaAbilityComponent.h"
#include "LastStandLegacyGameMode.generated.h"

class AZombie;

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
    TArray<AActor*> SpawnPoints;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Settings")
    float MinSafeDistance = 500.f;

    // لیستی تواناکان بۆ دابەشکردن بەسەر یاریزاناندا
    TArray<EHamaAbilityType> ActiveAbilities;

protected:
    virtual void BeginPlay() override;
    virtual void RestartPlayer(AController* NewPlayer) override;

private:
    FTimerHandle SpawnTimerHandle;

    void StartNextRound();
    void ProcessSpawning();
    AActor* PickWeightedSpawnPoint();

protected:
    UFUNCTION()
    void HandleZombieDeath(AZombie* DeadZombie, AController* KillerController);
};