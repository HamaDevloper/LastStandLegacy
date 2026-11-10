#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractInterface.h"
#include "Net/Core/PushModel/PushModel.h"
#include "GobbleGumTypes.h"
#include "GobbleGumMachine.generated.h"

class UStaticMeshComponent;
class UGobbleGumData;
class AHama;
class AHamaPlayerState;
class USoundBase;

UENUM(BlueprintType)
enum class EGobbleGumMachineState : uint8
{
    Idle,
    Rolling,
    Dispensed,
    Cooldown
};

UCLASS()
class LASTSTANDLEGACY_API AGobbleGumMachine : public AActor, public IInteractInterface
{
    GENERATED_BODY()

public:
    AGobbleGumMachine();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // جێبەجێکردنی ٤ فەنکشنەکەی IInteractInterface
    virtual void Interact(AHama* HamaChar) override;
    virtual FString GetInteractMessage(AHama* InteractingPlayer) override;
    virtual bool CanInteract(AHama* InteractingPlayer) override;
    virtual bool Client_PreInteract(AHama* Player) override;

    UFUNCTION(BlueprintPure, Category = "GobbleGum|Machine")
    EGobbleGumMachineState GetMachineState() const { return MachineState; }

    UFUNCTION(BlueprintPure, Category = "GobbleGum|Machine")
    int32 GetCurrentCost(const AHama* Player) const;

protected:
    void StartRolling(AHama* Buyer);
    void DispenseGum();
    void ResetMachine();

    UFUNCTION()
    void OnRep_MachineState();

    UFUNCTION()
    void OnRep_DispensedGum();

    void HandleVisualStateChanged(EGobbleGumMachineState NewState);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GobbleGum|Components")
    TObjectPtr<UStaticMeshComponent> MachineMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GobbleGum|Components")
    TObjectPtr<UStaticMeshComponent> GumBallMesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|Economy")
    int32 BaseCost = 500;

    UPROPERTY(EditDefaultsOnly, Category = "GobbleGum|Config")
    float InteractionDistance = 250.0f;

    UPROPERTY(EditDefaultsOnly, Category = "GobbleGum|Timers")
    float RollDuration = 3.0f;

    UPROPERTY(EditDefaultsOnly, Category = "GobbleGum|Timers")
    float PickupTimeout = 10.0f;

    UPROPERTY(EditDefaultsOnly, Category = "GobbleGum|Audio")
    TObjectPtr<USoundBase> RejectSound;

    UPROPERTY(EditDefaultsOnly, Category = "GobbleGum|Audio")
    TObjectPtr<USoundBase> RollSound;

    UPROPERTY(EditDefaultsOnly, Category = "GobbleGum|Audio")
    TObjectPtr<USoundBase> DispenseSound;

    UPROPERTY(ReplicatedUsing = OnRep_MachineState, BlueprintReadOnly, Category = "GobbleGum|State")
    EGobbleGumMachineState MachineState = EGobbleGumMachineState::Idle;

    UPROPERTY(ReplicatedUsing = OnRep_DispensedGum, BlueprintReadOnly, Category = "GobbleGum|State")
    TObjectPtr<UGobbleGumData> DispensedGum = nullptr;

    UPROPERTY()
    TWeakObjectPtr<AHama> CurrentBuyer = nullptr;

    FTimerHandle TimerHandle_Roll;
    FTimerHandle TimerHandle_Timeout;
};