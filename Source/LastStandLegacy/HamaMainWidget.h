#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HamaMainWidget.generated.h"

class UTextBlock;
class UImage;
class AHama;

UCLASS()
class LASTSTANDLEGACY_API UHamaMainWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // ئەم فەنکشنە تەنها یەک جار بانگ دەکرێت بۆ بەستنەوەی (Bind) ئیڤێنتەکان
    UFUNCTION(BlueprintCallable, Category = "UI|Initialization")
    void InitializeWidget(AHama* InHama);

protected:
    // ── بەستنەوەی دەقەکانی شاشەکە (دەبێت ناوەکانیان لەناو بلۆپرێنت هەمان شت بێت) ──
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
    AHama* CachedHamaChar;
};