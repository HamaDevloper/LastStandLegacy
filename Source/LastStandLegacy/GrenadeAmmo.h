// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractInterface.h"
#include "GrenadeAmmo.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class USoundBase;

UCLASS()
class LASTSTANDLEGACY_API AGrenadeAmmo : public AActor, public IInteractInterface
{
    GENERATED_BODY()

public:
    AGrenadeAmmo();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USphereComponent> CollisionComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MeshComp;

    UPROPERTY(EditDefaultsOnly, Category = "GrenadeAmmo|Audio")
    TObjectPtr<USoundBase> PurchaseSound;

    UPROPERTY(EditDefaultsOnly, Category = "GrenadeAmmo|Audio")
    TObjectPtr<USoundBase> RejectSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GrenadeAmmo|Settings")
    int32 GrenadeCost = 500;

public:
    virtual void Interact(AHama* InteractingPlayer) override;
    virtual FString GetInteractMessage(AHama* InteractingPlayer) override;
    virtual bool CanInteract(AHama* InteractingPlayer) override;
    virtual bool Client_PreInteract(AHama* InteractingPlayer) override;
};