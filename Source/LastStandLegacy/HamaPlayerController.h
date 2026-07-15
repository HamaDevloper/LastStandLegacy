#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HamaPlayerController.generated.h"

class UHamaMainWidget;
class AHama;
class AHamaPlayerState;

UCLASS()
class LASTSTANDLEGACY_API AHamaPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UHamaMainWidget> MainWidgetClass;

    UPROPERTY(BlueprintReadOnly, Category = "UI")
    UHamaMainWidget* MainWidgetRef;

protected:
    virtual void BeginPlay() override;

    virtual void OnRep_PlayerState() override;

    virtual void AcknowledgePossession(APawn* P) override;

private:
    void SetupClientUI();
};