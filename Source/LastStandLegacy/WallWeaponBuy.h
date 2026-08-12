#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractInterface.h"
#include "WallWeaponBuy.generated.h"

class ABaseWeapon;
class UBoxComponent;
class UStaticMeshComponent;
class AHama;

UCLASS()
class LASTSTANDLEGACY_API AWallWeaponBuy : public AActor, public IInteractInterface
{
    GENERATED_BODY()

public:
    AWallWeaponBuy();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> InteractBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> WeaponMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall Buy")
    TSubclassOf<ABaseWeapon> WeaponClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall Buy")
    FString WeaponName = "Weapon";

    // نرخی چەکەکە
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall Buy")
    int32 WeaponCost = 500;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall Buy")
    int32 AmmoCost = 250;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall Buy")
    int32 UpgradedAmmoCost = 4500;

    UPROPERTY(EditDefaultsOnly, Category = "Door|Audio")
    TObjectPtr<USoundBase> PurchaseSound;

    UPROPERTY(EditDefaultsOnly, Category = "Door|Audio")
    TObjectPtr<USoundBase> RejectSound;

public:
    virtual void Interact(AHama* HamaChar) override;
    virtual FString GetInteractMessage(AHama* InteractingPlayer) override;
    virtual bool CanInteract(AHama* InteractingPlayer) override;
    virtual bool Client_PreInteract(AHama* Player) override;
    virtual bool ShouldCancelReloadOnInteract() const override { return true; }

};