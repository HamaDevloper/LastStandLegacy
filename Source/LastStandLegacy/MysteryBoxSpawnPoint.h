#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MysteryBoxSpawnPoint.generated.h"

class USceneComponent;

UCLASS()
class LASTSTANDLEGACY_API AMysteryBoxSpawnPoint : public AActor
{
    GENERATED_BODY()

public:
    AMysteryBoxSpawnPoint();

    FORCEINLINE bool IsOccupied() const { return bIsOccupied; }
    FORCEINLINE void SetOccupied(bool bInOccupied) { bIsOccupied = bInOccupied; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> SpawnTransformComponent;

    bool bIsOccupied = false;
};