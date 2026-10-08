#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "HamaPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class AHama;
class UHamaMainWidget;
class URecoilComponent;

UCLASS()
class LASTSTANDLEGACY_API AHamaPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AHamaPlayerController();

    void OnOwnerDied();

    UFUNCTION(Server, Reliable)
    void Server_SpectateNext();

protected:
    virtual void BeginPlay() override;
    virtual void OnRep_PlayerState() override;
    virtual void AcknowledgePossession(APawn* P) override;
    virtual void SetupInputComponent() override;
    virtual void BeginSpectatingState() override;

    void CheckAndBindUI();
    AHama* GetNextSpectatorTarget(AHama* CurrentTarget);

    void SpectateNextActionPressed(const FInputActionValue& Value);

protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Input")
    TObjectPtr<UInputMappingContext> SpectatorMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Input")
    TObjectPtr<UInputAction> SpectateNextAction;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spectate")
    TObjectPtr<AHama> CurrentSpectateTarget;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<URecoilComponent> RecoilComponent;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UHamaMainWidget> MainWidgetClass;

    UPROPERTY()
    TObjectPtr<UHamaMainWidget> MainWidgetRef;
};