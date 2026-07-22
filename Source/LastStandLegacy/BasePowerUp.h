#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LastStandLegacyTypes.h"
#include "BasePowerUp.generated.h"

class AHama;
class USphereComponent;
class UStaticMeshComponent;

UCLASS(Abstract)
class LASTSTANDLEGACY_API ABasePowerUp : public AActor
{
    GENERATED_BODY()

public:
    ABasePowerUp();

protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditDefaultsOnly, Category = "PowerUp")
    EPowerUpType PowerUpType = EPowerUpType::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USphereComponent> CollisionSphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MeshComp;

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    virtual void ActivatePowerUp(AHama* Player);

private:
    bool bIsConsumed = false;
};