#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h" // یان CommonActivatableWidget.h ئەگەر سیستەمی Stack بەکاردەهێنیت
#include "LastStandLegacyTypes.h"
#include "HamaMainWidget.generated.h"

class UCommonTextBlock;
class UCommonActionWidget;
class UImage;
class UHorizontalBox;
class UWidgetAnimation;
class UMaterialInstanceDynamic;
class AHama;
class AHamaPlayerState;
class ALastStandLegacyGameState;

UCLASS(Abstract, BlueprintType, Blueprintable)
class LASTSTANDLEGACY_API UHamaMainWidget : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    void BindCharacter(AHama* InHama);
    void BindPlayerState(AHamaPlayerState* InPlayerState);
    void BindGameState(ALastStandLegacyGameState* InGameState);
    void UnbindAllEvents();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // --- Common UI Text Blocks (پشتگیری لە Text Styles و Formatting دەکەن) ---
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCommonTextBlock> Points;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCommonTextBlock> Kills;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCommonTextBlock> Round;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCommonTextBlock> Ammo;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCommonTextBlock> InteractText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCommonTextBlock> AmmoWarningText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCommonTextBlock> PingText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCommonTextBlock> ReviveText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCommonActionWidget> InteractActionWidget;

    // --- Images & Visuals ---
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> CrosshairImage;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> ReviveProgressBar;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> ReviveCenterIcon;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> PowerImage;

    // --- Containers ---
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UHorizontalBox> PerkContainer;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UHorizontalBox> GrenadeContainer;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UHorizontalBox> MonkeyContainer;

    // --- Animations ---
    UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
    TObjectPtr<UWidgetAnimation> ReviveAnim;

    UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
    TObjectPtr<UWidgetAnimation> PowerUpAnim;

    // --- Configuration Data ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI | PowerUps")
    TMap<EPowerUpType, TObjectPtr<UTexture2D>> PowerUpIcons;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI | Perks")
    TMap<FName, TObjectPtr<UTexture2D>> PerkIcons;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI | Perks")
    float Perksize = 128.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI | Perks")
    int32 MaxPerkSlots = 8;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI | Throwables")
    TObjectPtr<UTexture2D> GrenadeIconTexture;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI | Throwables")
    TObjectPtr<UTexture2D> MonkeyIconTexture;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI | Throwables")
    FVector2D ThrowableIconSize = FVector2D(32.0f, 32.0f);

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
    void UpdatePingDisplay();

    UFUNCTION()
    void OnPowerUpAnimFinished();

    UFUNCTION()
    void HandlePerksUpdate(const TArray<FName>& CurrentPerks);

    UFUNCTION()
    void NativeUpdateReviveProgress();

    UFUNCTION()
    void HandleDownStateChanged(bool bIsDowned);

    UFUNCTION()
    void ResetReviveUI();

    UFUNCTION()
    void HandleThrowableCountUpdate(int32 NewMonkeyCount, int32 NewGrenadeCount);

private:
    void EnsureThrowablePoolSize(UHorizontalBox* Container, TArray<TObjectPtr<UImage>>& Pool, UTexture2D* IconTexture, int32 TargetSize);

    UPROPERTY()
    TWeakObjectPtr<AHama> CachedHamaChar;

    UPROPERTY()
    TWeakObjectPtr<AHamaPlayerState> CachedHamaPS;

    UPROPERTY()
    TWeakObjectPtr<ALastStandLegacyGameState> CachedGameState;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> CachedReviveMID = nullptr;

    UPROPERTY()
    TArray<TObjectPtr<UImage>> PerkImagePool;

    UPROPERTY()
    TArray<TObjectPtr<UImage>> GrenadeImagePool;

    UPROPERTY()
    TArray<TObjectPtr<UImage>> MonkeyImagePool;

    FTimerHandle PingUpdateTimer;
    FTimerHandle PowerImageHideTimer;
    FTimerHandle UIUpdateTimerHandle;

    FWidgetAnimationDynamicEvent PowerUpAnimDelegate;

    UFUNCTION()
    void HandleReviveProgressUpdate(float Percent);

    UFUNCTION()
    void HandleReviveStateChanged(bool bIsBeingRevived, bool bIsRevivingOthers);
};