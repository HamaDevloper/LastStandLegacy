#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LastStandLegacyTypes.h"
#include "HamaMainWidget.generated.h"

class UTextBlock;
class UImage;
class AHama;
class AHamaPlayerState;
class ALastStandLegacyGameState;
class UHorizontalBox;

UCLASS()
class LASTSTANDLEGACY_API UHamaMainWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "UI|Initialization")
    void BindCharacter(AHama* InHama);

    void BindPlayerState(AHamaPlayerState* InPlayerState);
    void BindGameState(ALastStandLegacyGameState* InGameState);

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

protected:
    // --- UI Bindings ---
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Points;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Kills;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Round;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Ammo;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> CrosshairImage;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> InteractText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> AmmoWarningText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> PingText;

    // --- Animations & PowerUps ---
    UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
    TObjectPtr<UWidgetAnimation> PowerUpAnim;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> PowerImage;

    // 🔥 UE5 Modern Standard: بەکارهێنانی TObjectPtr لە ناو Mapەکاندا
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PowerUp Data")
    TMap<EPowerUpType, TObjectPtr<UTexture2D>> PowerUpIcons;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UHorizontalBox> PerkContainer;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perk Data")
    TMap<FName, TObjectPtr<UTexture2D>> PerkIcons;

    FWidgetAnimationDynamicEvent PowerUpAnimDelegate;

public:
    // --- Event Handlers ---
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

    UFUNCTION()
    void ShowPowerMessage(EPowerUpType PowerUpType);

    UFUNCTION()
    void OnPowerUpAnimFinished();

    UFUNCTION()
    void HandlePerksUpdate(const TArray<FName>& CurrentPerks);

private:
    UPROPERTY()
    TObjectPtr<AHama> CachedHamaChar;

    UPROPERTY()
    TObjectPtr<AHamaPlayerState> CachedHamaPS;

    UPROPERTY()
    TObjectPtr<ALastStandLegacyGameState> CachedGameState;

    UFUNCTION()
    void UpdatePingDisplay();

    void UnbindAllEvents();

    FTimerHandle PingUpdateTimer;
};