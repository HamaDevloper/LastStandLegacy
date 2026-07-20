#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HamaMainWidget.generated.h"

class UTextBlock;
class UImage;
class AHama;
class AHamaPlayerState;
class ALastStandLegacyGameState;

UCLASS()
class LASTSTANDLEGACY_API UHamaMainWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "UI|Initialization")
    void BindCharacter(AHama* InHama);

    void BindPlayerState(AHamaPlayerState* InPlayerState);
    void BindGameState(ALastStandLegacyGameState* InGameState);
    virtual void NativeDestruct() override;

protected:
    UPROPERTY(meta = (BindWidget))
    UTextBlock* Points;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* Kills;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* Round;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* Ammo;

    UPROPERTY(meta = (BindWidget))
    UImage* CrosshairImage;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* InteractText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* AmmoWarningText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* PingText;

public:
    UFUNCTION()
    void HandleAmmoUpdate(int32 CurrentAmmo, int32 ReserveAmmo);

    UFUNCTION()
    void HandleInteractUpdate(const FString& Message);

    UFUNCTION()
    void HandleCrosshairUpdate(bool bIsAimingAtEnemy);

    UFUNCTION()
    void HandlePointsUpdate(int32 NewPoints);

    UFUNCTION()
    void HandleKillsUpdate(int32 NewKills);

    UFUNCTION()
    void HandleRoundUpdate(int32 NewRound);

private:
    UPROPERTY()
    TObjectPtr<AHama> CachedHamaChar;

    UPROPERTY()
    TObjectPtr<AHamaPlayerState> CachedHamaPS;

    UFUNCTION()
    void UpdatePingDisplay();

    bool bIsGameStateBound = false;

    FTimerHandle PingUpdateTimer;
};