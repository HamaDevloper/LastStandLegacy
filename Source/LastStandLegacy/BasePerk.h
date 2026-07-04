// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractInterface.h"
#include "BasePerk.generated.h"

class AHama;

UCLASS(Abstract, Blueprintable)
class LASTSTANDLEGACY_API ABasePerk : public AActor, public IInteractInterface
{
    GENERATED_BODY()

public:
    ABasePerk();

protected:
    virtual void BeginPlay() override;

    // --- بەشی پێکهاتە فیزیکییەکان (Components) ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Perk|Components")
    class UBoxComponent* TriggerBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Perk|Components")
    class UStaticMeshComponent* PerkMachineMesh;

    // --- داتای پێرک (Perk Configuration) ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk|Settings")
    FName PerkID;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk|Settings")
    int32 PerkCost;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk|Settings")
    UStaticMesh* BottleMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perk|Settings")
    int32 SoloUsesLeftForQuickRevive = 3;

public:
    virtual void Interact(AHama* HamaChar) override;
    virtual FString GetInteractMessage() override;
    virtual bool CanInteract(AHama* InteractingPlayer) override;

    FName GetPerkID() const { return PerkID; }
    int32 GetPerkCost() const { return PerkCost; }
    UStaticMesh* GetBottleMesh() const { return BottleMesh; }
};