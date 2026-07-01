#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HamaMainWidget.generated.h" 

class AHama;

UCLASS()
class LASTSTANDLEGACY_API UHamaMainWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void UpdatePointsText(int32 NewPoints);
    void UpdateKillsText(int32 NewKills);
    void UpdateAmmoText(int32 CurrentAmmo, int32 ReserveAmmo);
    void UpdateRoundText(int32 NewRound);
    void InitializeWidget(class AHama* InHamaLat);

    // 🚀 لابردنی BlueprintImplementableEvent و گۆڕینیان بۆ فەنکشنی ئاسایی
    void ShowInteractMessage(const FString& Message);
    void HideInteractMessage();

    void ShowAmmoWarning(const FString& WarningMessage);
    void HideAmmoWarning();

protected:
    UPROPERTY(meta = (BindWidget))
    class UTextBlock* Points;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* Kills;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* Ammo;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* Round;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* InteractText;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* AmmoWarningText;

    UPROPERTY()
    TObjectPtr<AHama> CachedHamaChar;
};