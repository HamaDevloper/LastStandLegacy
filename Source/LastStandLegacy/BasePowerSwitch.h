#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractInterface.h"
#include "BasePowerSwitch.generated.h"

class UStaticMeshComponent;
class USoundBase;

UCLASS()
class LASTSTANDLEGACY_API ABasePowerSwitch : public AActor, public IInteractInterface
{
    GENERATED_BODY()

public:
    ABasePowerSwitch();


protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> SwitchFrameMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> SwitchLeverMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power Switch|Audio")
    TObjectPtr<USoundBase> PowerOnSound;

    UPROPERTY(ReplicatedUsing = OnRep_IsSwitchedOn)
    bool bIsSwitchedOn = false;

public:
    virtual void Interact(AHama* InteractingPlayer) override;
    virtual FString GetInteractMessage(AHama* InteractingPlayer) override;
    virtual bool CanInteract(AHama* InteractingPlayer) override;
    virtual bool Client_PreInteract(AHama* InteractingPlayer) override;

protected:
    UFUNCTION()
    void OnRep_IsSwitchedOn();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};