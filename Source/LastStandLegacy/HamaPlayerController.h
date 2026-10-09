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
    virtual void SetupInputComponent() override;
    virtual void AcknowledgePossession(APawn* P) override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void ClientRestart_Implementation(APawn* NewPawn) override;

    // Spectating Network API
    void OnOwnerDied();

    UFUNCTION(Server, Reliable)
    void Server_SpectateNext();

    UFUNCTION(Client, Reliable)
    void ClientEnterSpectating(AHama* Target);

    UFUNCTION(Client, Reliable)
    void Client_SetSpectateTarget(AHama* Target);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<URecoilComponent> RecoilComponent;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputMappingContext> SpectatorMappingContext;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> SpectateNextAction;

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
    AHama* GetNextSpectatorTarget(AHama* CurrentTarget);
    void ValidateSpectateTarget();
    void CheckAndBindUI();
    void SafeAddMappingContext(UInputMappingContext* Context, int32 Priority);
    void SafeRemoveMappingContext(UInputMappingContext* Context);
};