#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BaseDoor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class AHama;
class AHamaPlayerState;
class USoundBase;

UCLASS()
class LASTSTANDLEGACY_API ABaseDoor : public AActor
{
    GENERATED_BODY()

public:
    ABaseDoor();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintCallable, Category = "Door")
    bool CanInteract(AHama* InteractingPlayer);

    UFUNCTION(BlueprintCallable, Category = "Door")
    void Interact(AHama* Player);

    UFUNCTION(BlueprintCallable, Category = "Door")
    FString GetInteractMessage() const;

    UPROPERTY(EditDefaultsOnly, Category = "Door|Config")
    int32 DoorPrice = 750;

    UPROPERTY(ReplicatedUsing = OnRep_OpenDoor, BlueprintReadOnly, Category = "Door|State")
    bool bIsDoorOpen = false;

protected:
    UFUNCTION()
    void OnRep_OpenDoor();

    UFUNCTION(BlueprintImplementableEvent, Category = "Door")
    void EventOnDoorOpened(bool bIsOpen);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Components")
    TObjectPtr<UStaticMeshComponent> DoorMesh;

    UPROPERTY(EditDefaultsOnly, Category = "Door|Audio")
    TObjectPtr<USoundBase> DoorOpenSound;

private:
    void OpenDoor(AHama* Player, AHamaPlayerState* PS);
};