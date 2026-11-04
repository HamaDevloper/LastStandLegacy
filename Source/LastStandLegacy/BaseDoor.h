#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractInterface.h"
#include "BaseDoor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class AHama;
class AHamaPlayerState;
class USoundBase;

UCLASS()
class LASTSTANDLEGACY_API ABaseDoor : public AActor, public IInteractInterface
{
    GENERATED_BODY()

public:
    ABaseDoor();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    virtual void Interact(AHama* HamaChar) override;
    virtual FString GetInteractMessage(AHama* InteractingPlayer) override;
    virtual bool CanInteract(AHama* InteractingPlayer) override;
    virtual bool Client_PreInteract(AHama* Player) override;

    UPROPERTY(EditDefaultsOnly, Category = "Door|Config")
    int32 DoorPrice = 750;

    UPROPERTY(ReplicatedUsing = OnRep_OpenDoor, BlueprintReadOnly, Category = "Door|State")
    bool bIsDoorOpen = false;

    // ژمارەی ئەو ژوورەی کە ئەم دەرگایە دەیکاتەوە (لەناو مەپەکە بۆ هەر دەرگایەک دیاری دەکەیت: 1، 2، 3...)
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Door|Zones")
    int32 UnlockZoneID = 1;

    // ئیختیاری: ئەگەر دەرگاکە دوو ژووری جیاوازی بەیەکەوە دەبەستەوە کە لە دوو لای جیاوازەوە دەکرێنەوە (0 واتە ناچالاک)
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Door|Zones")
    int32 SecondaryZoneID = 0;

protected:
    UFUNCTION()
    void OnRep_OpenDoor();

    UFUNCTION(BlueprintImplementableEvent, Category = "Door")
    void EventOnDoorOpened(bool bIsOpen);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Components")
    TObjectPtr<UStaticMeshComponent> DoorMesh;

    UPROPERTY(EditDefaultsOnly, Category = "Door|Audio")
    TObjectPtr<USoundBase> DoorOpenSound;

    UPROPERTY(EditDefaultsOnly, Category = "Door|Audio")
    TObjectPtr<USoundBase> RejectSound;

private:
    void OpenDoor(AHama* Player, AHamaPlayerState* PS);
};