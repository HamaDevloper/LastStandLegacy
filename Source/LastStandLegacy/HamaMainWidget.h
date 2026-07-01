#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HamaMainWidget.generated.h" // تێبینی بکە TextBlock لێرە سڕاوەتەوە

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

    UFUNCTION(BlueprintImplementableEvent, Category = "UI")
    void ShowInteractMessage(const FString& Message);

    UFUNCTION(BlueprintImplementableEvent, Category = "UI")
    void HideInteractMessage();

    UFUNCTION(BlueprintImplementableEvent, Category = "UI")
    void ShowAmmoWarning(const FString& WarningMessage);

    UFUNCTION(BlueprintImplementableEvent, Category = "UI")
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

    UPROPERTY()
    TObjectPtr<AHama> CachedHamaChar;
};