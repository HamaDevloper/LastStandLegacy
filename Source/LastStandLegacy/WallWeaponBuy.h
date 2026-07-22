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

    // ئەو چەکەی کە دەفرۆشرێت
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall Buy")
    TSubclassOf<ABaseWeapon> WeaponClass;

    // ناوی چەکەکە بۆ سەر شاشە
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall Buy")
    FString WeaponName = "Weapon";

    // نرخی چەکەکە
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall Buy")
    int32 WeaponCost = 500;

    // نرخی فیشەک (ئەگەر یاریزانەکە چەکەکەی هەبوو)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wall Buy")
    int32 AmmoCost = 250;

public:
    virtual void Interact(AHama* HamaChar) override;
    virtual FString GetInteractMessage() override;
    virtual bool CanInteract(AHama* InteractingPlayer) override;
};