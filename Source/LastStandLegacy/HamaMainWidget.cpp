#include "HamaMainWidget.h"
#include "Hama.h"
#include "LastStandLegacyGameState.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

// -------------------------------------------------------------------------
// Initialization (0-Coupling Architecture)
// -------------------------------------------------------------------------
void UHamaMainWidget::InitializeWidget(AHama* InHama)
{
    if (!InHama) return;

    CachedHamaChar = InHama;

    InHama->OnAmmoUpdateEvent.BindUObject(this, &UHamaMainWidget::HandleAmmoUpdate);
    InHama->OnInteractUpdateEvent.BindUObject(this, &UHamaMainWidget::HandleInteractUpdate);
    InHama->OnCrosshairUpdateEvent.BindUObject(this, &UHamaMainWidget::HandleCrosshairUpdate);
    InHama->OnPointsUpdateEvent.BindUObject(this, &UHamaMainWidget::HandlePointsUpdate);
    InHama->OnKillsUpdateEvent.BindUObject(this, &UHamaMainWidget::HandleKillsUpdate);

    if (UWorld* World = GetWorld())
    {
        if (ALastStandLegacyGameState* GS = World->GetGameState<ALastStandLegacyGameState>())
        {
            GS->OnRoundChangedDelegate.AddUObject(this, &UHamaMainWidget::HandleRoundUpdate);

            HandleRoundUpdate(GS->GetCurrentRound());
        }
    }

    if (InteractText) InteractText->SetVisibility(ESlateVisibility::Hidden);
    if (AmmoWarningText) AmmoWarningText->SetVisibility(ESlateVisibility::Hidden);
}

// -------------------------------------------------------------------------
// Event Handlers (ئەم فەنکشنانە خۆکارانە کار دەکەن کاتێک داتا دەگۆڕێت)
// -------------------------------------------------------------------------
void UHamaMainWidget::HandlePointsUpdate(int32 NewPoints)
{
    if (Points)
    {
        Points->SetText(FText::AsNumber(NewPoints));
    }
}

void UHamaMainWidget::HandleKillsUpdate(int32 NewKills)
{
    if (Kills)
    {
        Kills->SetText(FText::AsNumber(NewKills));
    }
}

void UHamaMainWidget::HandleRoundUpdate(int32 NewRound)
{
    if (Round)
    {
        Round->SetText(FText::AsNumber(NewRound));
    }
}

void UHamaMainWidget::HandleAmmoUpdate(int32 CurrentAmmo, int32 ReserveAmmo)
{
    if (!Ammo) return;

    if (CachedHamaChar && CachedHamaChar->GetDeathMachine())
    {
        // هێمای ئینفینیتی کاتێک چەکی Death Machineـی پێیە
        static const FText InfinityText = FText::FromString(TEXT("\u221E / \u221E"));
        Ammo->SetText(InfinityText);

        if (AmmoWarningText) AmmoWarningText->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    FText FormattedAmmo = FText::Format(FText::FromString(TEXT("{0} / {1}")), CurrentAmmo, ReserveAmmo);
    Ammo->SetText(FormattedAmmo);

    // لۆژیکی هۆشداریدانی فیشەک (Low Ammo / No Ammo)
    if (AmmoWarningText && CachedHamaChar && CachedHamaChar->GetCurrentWeapon())
    {
        int32 MaxClipSize = CachedHamaChar->GetCurrentWeapon()->GetMaxClipAmmo();
        int32 LowAmmoThreshold = FMath::RoundToInt(MaxClipSize * 0.25f);

        if (CurrentAmmo == 0 && ReserveAmmo <= 0)
        {
            AmmoWarningText->SetText(FText::FromString(TEXT("No Ammo!")));
            AmmoWarningText->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
            AmmoWarningText->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else if (CurrentAmmo <= LowAmmoThreshold && CurrentAmmo > 0)
        {
            AmmoWarningText->SetText(FText::FromString(TEXT("LOW AMMO")));
            AmmoWarningText->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
            AmmoWarningText->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else
        {
            AmmoWarningText->SetVisibility(ESlateVisibility::Hidden);
        }
    }
}

void UHamaMainWidget::HandleInteractUpdate(const FString& Message)
{
    if (InteractText)
    {
        if (Message.IsEmpty())
        {
            InteractText->SetVisibility(ESlateVisibility::Hidden);
        }
        else
        {
            InteractText->SetText(FText::FromString(Message));
            InteractText->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
    }
}

void UHamaMainWidget::HandleCrosshairUpdate(bool bIsAimingAtEnemy)
{
    if (CrosshairImage)
    {
        if (bIsAimingAtEnemy)
        {
            CrosshairImage->SetColorAndOpacity(FLinearColor::Red);
        }
        else
        {
            CrosshairImage->SetColorAndOpacity(FLinearColor::White);
        }
    }
}