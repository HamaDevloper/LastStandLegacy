#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LastStandLegacyGameMode.generated.h"

class AZombie;

UCLASS()
class LASTSTANDLEGACY_API ALastStandLegacyGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ALastStandLegacyGameMode();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Round")
    int32 CurrentRound = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Round")
    int32 ZombiesToKill = 10;

    int32 DeadZombiesCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    TSubclassOf<AZombie> ZombieClass;

    // Spawn Points لە Details Panel زیاد دەکرێن
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    TArray<AActor*> SpawnPoints;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    float MinSafeDistance = 500.f;



protected:
    virtual void BeginPlay() override;

private:
    void StartNextRound();
    void SpawnZombiesForRound();
    AActor* PickWeightedSpawnPoint();

    // لە بەشی public یان protected
protected:
    UFUNCTION()
    void HandleZombieDeath(AZombie* DeadZombie); // زیادکردنی ئەمە
};