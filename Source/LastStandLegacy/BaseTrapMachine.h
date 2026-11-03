#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractInterface.h"
#include "Net/Core/PushModel/PushModel.h"
#include "BaseTrapMachine.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class AHama;
class USoundBase;

UENUM(BlueprintType)
enum class ETrapMachineState : uint8
{
    Off UMETA(DisplayName = "Off"),
    Ready UMETA(DisplayName = "Ready"),
    Active UMETA(DisplayName = "Active"),
    CoolDown UMETA(DisplayName = "Cool Down")
};

UCLASS(Abstract)
class LASTSTANDLEGACY_API ABaseTrapMachine : public AActor, public IInteractInterface
{
    GENERATED_BODY()

public:
    ABaseTrapMachine();

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> RootComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MachineMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USphereComponent> InteractCollision;

    // ڕیپلیکەیشن بە شێوازی Push Model
    UPROPERTY(ReplicatedUsing = OnRep_TrapState, BlueprintReadOnly, Category = "Trap|State")
    ETrapMachineState TrapState;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trap|Config")
    int32 ActivationCost;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trap|Config")
    float ActiveDuration;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trap|Config")
    float CooldownDuration;

    UPROPERTY(EditDefaultsOnly, Category = "Trap|Config")
    TObjectPtr<USoundBase> RejectSound;

    FTimerHandle TimerHandle_ActiveDuration;
    FTimerHandle TimerHandle_CooldownDuration;

    UFUNCTION()
    virtual void OnRep_TrapState();

    // فەنکشنە سەرەکییەکانی سێرڤەر (قەدەغەیە کلاسی منداڵ دەستکارییان بکات)
    void ActivateTrap_Server();
    void CooldownTrap_Server();
    void ResetTrap_Server();

    // ئەم فەنکشنانە virtual دەبن و منداڵ دەتوانێت بە ئارەزووی خۆی ئۆڤەرڕایدیان بکات
    // پێویستیش ناکات منداڵەکە بیری بێت Super بانگ بکات
    virtual void OnTrapActivated_Server() {}
    virtual void OnTrapCooldown_Server() {}
    virtual void OnTrapReset_Server() {}

    virtual void HandleStateVisuals_Client(ETrapMachineState NewState);

    void SetTrapState_Server(ETrapMachineState NewState);

public:
    virtual void Interact(AHama* HamaChar) override;
    virtual FString GetInteractMessage(AHama* InteractingPlayer) override;
    virtual bool CanInteract(AHama* InteractingPlayer) override;
    virtual bool Client_PreInteract(AHama* Player) override;
};