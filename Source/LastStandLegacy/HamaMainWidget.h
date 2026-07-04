#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HamaMainWidget.generated.h"

// ناساندنی کڵاسەکان لە سەرەوە بۆ خاوێنی کۆدەکە
class AHama;
class UTextBlock;
class UImage;

UCLASS()
class LASTSTANDLEGACY_API UHamaMainWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // --- بەشی سەرەتایی (Initialization) ---
    void InitializeWidget(AHama* InHama);

    // --- بەشی نوێکردنەوەی زانیارییەکانی شاشە (HUD Updates) ---
    void UpdatePointsText(int32 NewPoints);
    void UpdateKillsText(int32 NewKills);
    void UpdateAmmoText(int32 CurrentAmmo, int32 ReserveAmmo);
    void UpdateRoundText(int32 NewRound);
    void UpdateCrosshairState(bool bIsAimingAtEnemy);

    // --- بەشی نامە و ئاگادارکردنەوەکان (Messages & Warnings) ---
    void ShowInteractMessage(const FString& Message);
    void HideInteractMessage();
    void ShowAmmoWarning(const FString& WarningMessage);
    void HideAmmoWarning();

protected:
    // --- پێکهاتەکانی دیزاین (UI Components) ---
    UPROPERTY(meta = (BindWidget))
    UTextBlock* Points;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* Kills;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* Ammo;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* Round;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* InteractText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* AmmoWarningText;

    UPROPERTY(meta = (BindWidget))
    UImage* CrosshairImage;

    // --- داتای هەڵگیراو (Cached Data) ---
    UPROPERTY()
    TObjectPtr<AHama> CachedHamaChar;
};