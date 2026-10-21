#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "HamaPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class URecoilComponent;
class UHamaMainWidget;
class AHama;

UCLASS()
class LASTSTANDLEGACY_API AHamaPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AHamaPlayerController();

    virtual void BeginPlay() override;
    virtual void OnRep_PlayerState() override;
    virtual void OnRep_Pawn() override;
    virtual void SetupInputComponent() override;
    virtual void AcknowledgePossession(APawn* P) override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void ClientRestart_Implementation(APawn* NewPawn) override;

    void OnOwnerDied();

    UFUNCTION(Server, Reliable)
    void Server_CycleSpectateTarget(int32 Direction);

    UFUNCTION(Client, Reliable)
    void ClientEnterSpectating(AHama* Target);

    UFUNCTION(Client, Reliable)
    void Client_SetSpectateTarget(AHama* Target);

    void InitializeLocalPawn(APawn* InPawn);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<URecoilComponent> RecoilComponent;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputMappingContext> SpectatorMappingContext;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> SpectateNextAction;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> SpectatePreviousAction;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UHamaMainWidget> MainWidgetClass;

    UPROPERTY()
    TObjectPtr<UHamaMainWidget> MainWidgetRef;

    UPROPERTY()
    TObjectPtr<AHama> CurrentSpectateTarget;

    UPROPERTY()
    TObjectPtr<UInputMappingContext> ActiveGameplayContext;

    FTimerHandle SpectateValidationTimerHandle;

private:
    void SpectateNextActionPressed(const FInputActionValue& Value);
    void SpectatePreviousActionPressed(const FInputActionValue& Value);
    void RequestSpectateCycle(int32 Direction);

    AHama* GetCycledSpectatorTarget(AHama* CurrentTarget, int32 Direction) const;
    void ApplyLocalSpectateViewTarget(AHama* NewTarget);

    void ValidateSpectateTarget();
    void CheckAndBindUI();
    void SafeAddMappingContext(UInputMappingContext* Context, int32 Priority);
    void SafeRemoveMappingContext(UInputMappingContext* Context);

    virtual void ReceivedPlayer() override;
    void HandleGameStateSet(AGameStateBase* NewGameState);

    TWeakObjectPtr<ALastStandLegacyGameState> BoundGameState;
    TWeakObjectPtr<AHamaPlayerState> BoundPlayerState;
    TWeakObjectPtr<AHama> BoundCharacter;

    float LastClientSwitchTime = -1.0f;
    float LastServerSwitchTime = -1.0f;
    static constexpr float SpectateSwitchCooldown = 0.15f;
};