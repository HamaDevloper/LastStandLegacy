// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractInterface.h"
#include "BasePerk.generated.h"

class AHama;
class UBoxComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class USoundBase;

UCLASS(Abstract, Blueprintable)
class LASTSTANDLEGACY_API ABasePerk : public AActor, public IInteractInterface
{
    GENERATED_BODY()

public:
    ABasePerk();

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // --- پێکهاتە فیزیکی و بینراوەکان (Components) ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Perk|Components")
    TObjectPtr<UBoxComponent> TriggerBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Perk|Components")
    TObjectPtr<UStaticMeshComponent> PerkMachineMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Perk|Components")
    TObjectPtr<UPointLightComponent> MachineLight;

    // --- دۆخی تۆڕ (Push Model State) ---
    UPROPERTY(ReplicatedUsing = OnRep_IsDepleted)
    bool bIsDepleted = false;

    UFUNCTION()
    void OnRep_IsDepleted();

    // --- ڕێکخستنەکانی پێرک (Perk Configuration) ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk|Settings")
    FName PerkID;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk|Settings")
    int32 PerkCost;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk|Settings")
    TObjectPtr<UStaticMesh> BottleMesh;

    UPROPERTY(EditDefaultsOnly, Category = "Perk|Audio")
    TObjectPtr<USoundBase> RejectSound;

    UPROPERTY(EditDefaultsOnly, Category = "Perk|Audio")
    TObjectPtr<USoundBase> PowerDownSound;

    int32 SoloUsesLeftForQuickRevive = 3;

public:
    virtual void Interact(AHama* HamaChar) override;
    virtual FString GetInteractMessage(AHama* InteractingPlayer) override;
    virtual bool CanInteract(AHama* InteractingPlayer) override;
    virtual bool Client_PreInteract(AHama* Player) override;
    virtual bool ShouldCancelReloadOnInteract() const override { return true; }

    FName GetPerkID() const { return PerkID; }
    int32 GetPerkCost() const { return PerkCost; }
    UStaticMesh* GetBottleMesh() const { return BottleMesh; }

private:
    static constexpr float MaxInteractDistanceSq = 250.0f * 250.0f;
};