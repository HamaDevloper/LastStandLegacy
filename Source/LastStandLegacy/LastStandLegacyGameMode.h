#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "LastStandLegacyGameMode.generated.h"

class AZombie;
class AZombieSpawnPoint;
class ABasePowerUp;
class AController;
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

public:
    // [Zombie Settings] 
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

    // لێرەدا لۆکاڵی تەنها بۆ سێرڤەر بەکاری دەهێنین، پێویستی بە OnRep نییە
    int32 CurrentRound = 1;

    int32 ZombiesSpawnedThisRound = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LastStandLegacyGameMode|Zombie Settings")
    int32 ZombiesSpawnLimit = 24;

    // [PowerUp Settings]
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LastStandLegacyGameMode|PowerUp")
    TArray<TSubclassOf<ABasePowerUp>> PowerUpClasses;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LastStandLegacyGameMode|PowerUp")
    int32 MaxPowerSpawn = 5;

    int32 CurrentPowerSpawn = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LastStandLegacyGameMode|PowerUp", meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float PowerUpDropChance = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LastStandLegacyGameMode|PowerUp")
    float PowerUpCooldownTime = 10.0f;

    float CurrentPowerSpawnTime = -9999.0f;

    // [Abilities]
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LastStandLegacyGameMode|Abilities")
    TArray<EHamaAbilityType> ActiveAbilities;

private:
    FTimerHandle SpawnTimerHandle;
};