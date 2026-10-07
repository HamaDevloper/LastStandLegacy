#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HamaPlayerController.generated.h"

class UHamaMainWidget;
class AHama;
class AHamaPlayerState;
class ALastStandLegacyGameState;
class URecoilComponent;

UCLASS()
class LASTSTANDLEGACY_API AHamaPlayerController : public APlayerController
{
    GENERATED_BODY()

public:

    AHamaPlayerController();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UHamaMainWidget> MainWidgetClass;

    UPROPERTY(BlueprintReadOnly, Category = "UI")
    UHamaMainWidget* MainWidgetRef;

protected:
    virtual void BeginPlay() override;
    virtual void OnRep_PlayerState() override;
    virtual void AcknowledgePossession(APawn* P) override;
    void CheckAndBindUI();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<URecoilComponent> RecoilComponent;

private:
    FTimerHandle GameStateBindTimer;

    UPROPERTY()
    TObjectPtr<AHama> CurrentSpectateTarget;

public:
    void OnOwnerDied();

protected:
    AHama* GetNextSpectatorTarget(AHama* CurrentTarget = nullptr);
    virtual void ClientGotoState_Implementation(FName NewState, uint8 GroupID) override;
};