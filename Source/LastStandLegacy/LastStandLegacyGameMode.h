#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "LastStandLegacyGameMode.generated.h"

class AZombie;
class AZombieSpawnPoint;
class ABasePowerUp;
class AController;
class ABasePerk;
class APerkSpawnPoint;
enum class EHamaAbilityType : uint8;

UCLASS()
class LASTSTANDLEGACY_API ALastStandLegacyGameMode : public AGameMode
{
    GENERATED_BODY()

public:
    ALastStandLegacyGameMode();

    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void BeginPlay() override;

    void HandleZombieDeath(AZombie* DeadZombie, AController* KillerController);
    void ActivateNuke();

protected:
    void SpawnPowers(FVector SpawnLocation);
    void StartNextRound();
    void ProcessSpawning();
    AActor* PickWeightedSpawnPoint();

    void MyShufflePerks(TArray<TSubclassOf<ABasePerk>>& ArrayToShuffle);
    void SpawnRandomPerks();

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LastStandLegacyGameMode|Zombie Settings")
    TSubclassOf<AZombie> ZombieClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LastStandLegacyGameMode|Zombie Settings")
    TArray<AZombieSpawnPoint*> SpawnPoints;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LastStandLegacyGameMode|Zombie Settings")
    float MinSafeDistance = 500.f;

    int32 DeadZombiesCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LastStandLegacyGameMode|Zombie Settings")
    int32 ActiveZombiesCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LastStandLegacyGameMode|Zombie Settings")
    int32 ZombiesToKill = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LastStandLegacyGameMode|Zombie Settings")
    int32 CurrentRound = 1;

    int32 ZombiesSpawnedThisRound = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LastStandLegacyGameMode|Zombie Settings")
    int32 ZombiesSpawnLimit = 30;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LastStandLegacyGameMode|Zombie Settings")
    int32 StartNexRoundDelay = 5;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zombies|Zombie Settings")
    int32 BaseZombiesCount = 10;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zombies|Zombie Settings")
    int32 ZombiesPerRoundIncrement = 5;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy|Scaling")
    int32 BaseStartingPoints = 500;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy|Scaling")
    int32 PointsPerRoundScaling = 250;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LastStandLegacyGameMode|PowerUp")
    TArray<TSubclassOf<ABasePowerUp>> PowerUpClasses;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LastStandLegacyGameMode|Perks")
    TArray<TSubclassOf<ABasePerk>> PerkClasses;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LastStandLegacyGameMode|PowerUp")
    int32 MaxPowerSpawn = 5;

    int32 CurrentPowerSpawn = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LastStandLegacyGameMode|PowerUp", meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float PowerUpDropChance = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LastStandLegacyGameMode|PowerUp")
    float PowerUpCooldownTime = 10.0f;

    float CurrentPowerSpawnTime = -9999.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LastStandLegacyGameMode|Abilities")
    TArray<EHamaAbilityType> ActiveAbilities;

private:
    FTimerHandle SpawnTimerHandle;

    TArray<AZombie*> ZombiesToNuke;
    FTimerHandle NukeTimerHandle;
    bool bIsNuking = false;
    int32 DynamicKillBatchSize = 1;

    UFUNCTION()
    void ProcessNukeKills();
    void RespawnDeadPlayers();

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zombie Spawning|Timing")
    float BaseSpawnInterval = 2.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zombie Spawning|Timing")
    float SpawnIntervalDecreasePerRound = 0.2f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zombie Spawning|Timing")
    float MinSpawnInterval = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    float TeammateSpawnRadius = 300.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    int32 MaxSpawnAttempts = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    float MaxSpawnHeightDiff = 150.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    float FallbackDistance = 150.f;

    float GetCalculateSpawnInterval() const;

private:
    FTimerHandle RoundTransitionTimerHandle;


public:
    void CheckGameOverCondition();

    virtual void Logout(AController* Exiting) override;
    virtual void RestartPlayer(AController* NewPlayer) override;

protected:
    void TriggerGameOver();
    bool GetSpawnTransformNearTeammate(AController* Player, FTransform& OutSpawnTransform);
};