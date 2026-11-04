#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZombieSpawnPoint.generated.h"

UCLASS()
class LASTSTANDLEGACY_API AZombieSpawnPoint : public AActor
{
    GENERATED_BODY()

public:
    AZombieSpawnPoint();
  
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Spawn Settings")
    int32 ZoneID = 0;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Spawn Settings")
    bool bIsActive = false;

    FORCEINLINE bool IsSpawnActive() const { return bIsActive; }
    FORCEINLINE void SetSpawnActive(bool bInActive) { bIsActive = bInActive; }
};