#include "HamaMainWidget.h"
#include "Hama.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

// -------------------------------------------------------------------------
// Initialization
// -------------------------------------------------------------------------
void UHamaMainWidget::InitializeWidget(AHama* InHama)
{
    CachedHamaChar = InHama;
}

// -------------------------------------------------------------------------
// HUD Updates
// -------------------------------------------------------------------------
void UHamaMainWidget::UpdatePointsText(int32 NewPoints)
{
    if (Points)
    {
        Points->SetText(FText::AsNumber(NewPoints));
    }
}

void UHamaMainWidget::UpdateKillsText(int32 NewKills)
{
    if (Kills)
    {
        Kills->SetText(FText::AsNumber(NewKills));
    }
}

void UHamaMainWidget::UpdateRoundText(int32 NewRound)
{
    if (Round)
    {
        Round->SetText(FText::AsNumber(NewRound));
    }
}

void UHamaMainWidget::UpdateAmmoText(int32 CurrentAmmo, int32 ReserveAmmo)
{
    if (!Ammo) return;

    if (CachedHamaChar && CachedHamaChar->GetDeathMachine())
    {
        static const FText InfinityText = FText::FromString(TEXT("\u221E / \u221E"));
        Ammo->SetText(InfinityText);
        HideAmmoWarning();
        return;
    }

    FText FormattedAmmo = FText::Format(FText::FromString(TEXT("{0} / {1}")), CurrentAmmo, ReserveAmmo);
    Ammo->SetText(FormattedAmmo);
}

void UHamaMainWidget::UpdateCrosshairState(bool bIsAimingAtEnemy)
{
    if (!CrosshairImage) return;

    if (bIsAimingAtEnemy)
    {
        CrosshairImage->SetColorAndOpacity(FLinearColor::Red);
    }
    else
    {
        CrosshairImage->SetColorAndOpacity(FLinearColor::White);
    }
}

// -------------------------------------------------------------------------
// Messages & Warnings
// -------------------------------------------------------------------------
void UHamaMainWidget::ShowInteractMessage(const FString& Message)
{
    if (InteractText)
    {
        InteractText->SetText(FText::FromString(Message));
        InteractText->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
}

void UHamaMainWidget::HideInteractMessage()
{
    if (InteractText)
    {
        InteractText->SetVisibility(ESlateVisibility::Hidden);
    }
}

void UHamaMainWidget::ShowAmmoWarning(const FString& WarningMessage)
{
    if (AmmoWarningText)
    {
        AmmoWarningText->SetText(FText::FromString(WarningMessage));
        AmmoWarningText->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
        AmmoWarningText->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
}

void UHamaMainWidget::HideAmmoWarning()
{
    if (AmmoWarningText)
    {
        AmmoWarningText->SetVisibility(ESlateVisibility::Hidden);
    }
}