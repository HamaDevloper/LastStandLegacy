// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractInterface.h"
#include "GambleMachine.generated.h"

class UStaticMeshComponent;
class AHama;
class USoundBase;

UENUM(BlueprintType)
enum class EGambleState : uint8
{
    Idle,       
    Gambling,   
    CoolDown,
    Ready
};

UCLASS()
class LASTSTANDLEGACY_API AGambleMachine : public AActor, public IInteractInterface
{
	GENERATED_BODY()
	
public:	
	AGambleMachine();

    virtual bool CanInteract(AHama* InteractingPlayer);
    virtual bool Client_PreInteract(AHama* InteractingPlayer);
    virtual void Interact(AHama* InteractingPlayer); 
    virtual FText GetInteractMessage(AHama* InteractingPlayer);

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gamble")
    TObjectPtr<UStaticMeshComponent> GambleMesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gamble|Settings", meta = (ClampMin = "0"))
    int32 GamblePrice = 1000;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gamble|Settings")
    TObjectPtr<USoundBase> RejectSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gamble|Timing", meta = (ClampMin = "0.01"))
    float GambleDuration = 5.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gamble|Timing", meta = (ClampMin = "0.01"))
    float ReadyDuration = 5.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gamble|Timing", meta = (ClampMin = "0.01"))
    float CooldownDuration = 2.0f;

    UPROPERTY(ReplicatedUsing = OnRep_MachineState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gamble|State")
    EGambleState MachineState = EGambleState::Idle;

    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gamble|State")
    TObjectPtr<AHama> CurrentGambler;

    void StartGamble(AHama* Gambler);
    void FinishGamble();
    void GivePrize(AHama* ClaimingPlayer);
    void ResetGambleMachine();
    void StartCooldown();

    UFUNCTION()
    void OnRep_MachineState();

private:
    FTimerHandle StartGambleTimer;
    FTimerHandle EndGambleTimer;
};