#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractInterface.h"
#include "BaseDoor.generated.h"

class AHama;
class UStaticMeshComponent;
class UBoxComponent;

UCLASS()
class LASTSTANDLEGACY_API ABaseDoor : public AActor, public IInteractInterface
{
    GENERATED_BODY()

public:
    ABaseDoor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Components")
    TObjectPtr<UStaticMeshComponent> DoorMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Components")
    TObjectPtr<UBoxComponent> TriggerBox;

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    virtual void Interact(AHama* Player) override;
    virtual FString GetInteractMessage() override;
    virtual bool CanInteract(AHama* InteractingPlayer) override;

    UPROPERTY(ReplicatedUsing = OnRep_OpenDoor, BlueprintReadOnly, Category = "Door|Settings")
    bool bIsDoorOpen = false;

    UPROPERTY(EditDefaultsOnly, Category = "Door|Settings")
    int32 DoorPrice = 1000;

    UFUNCTION()
    void OpenDoor(AHama* Player);

    UFUNCTION()
    void OnRep_OpenDoor();

    UFUNCTION(BlueprintImplementableEvent)
    void EventOnDoorOpened(bool bIsOpen);
};