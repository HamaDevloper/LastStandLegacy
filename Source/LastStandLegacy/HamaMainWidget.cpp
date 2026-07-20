#include "HamaMainWidget.h"
#include "Hama.h"
#include "LastStandLegacyGameState.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "HamaPlayerState.h"
#include "BaseWeapon.h"

// -------------------------------------------------------------------------
// Initialization & Bindings (Separated Concerns)
// -------------------------------------------------------------------------

void UHamaMainWidget::BindCharacter(AHama* InHama)
{
    if (!InHama) return;
    CachedHamaChar = InHama;

    InHama->OnAmmoUpdateEvent.Unbind();
    InHama->OnInteractUpdateEvent.Unbind();
    InHama->OnCrosshairUpdateEvent.Unbind();

    InHama->OnAmmoUpdateEvent.BindUObject(this, &UHamaMainWidget::HandleAmmoUpdate);
    InHama->OnInteractUpdateEvent.BindUObject(this, &UHamaMainWidget::HandleInteractUpdate);
    InHama->OnCrosshairUpdateEvent.BindUObject(this, &UHamaMainWidget::HandleCrosshairUpdate);

    if (ABaseWeapon* CurrentWep = InHama->GetCurrentWeapon())
    {
        HandleAmmoUpdate(CurrentWep->GetCurrentAmmo(), CurrentWep->GetReserveAmmo());
    }
}
void UHamaMainWidget::BindPlayerState(AHamaPlayerState* InPlayerState)
{
    if (!InPlayerState || CachedHamaPS == InPlayerState) return;

    CachedHamaPS = InPlayerState;

    InPlayerState->OnPointsChanged.BindUObject(this, &UHamaMainWidget::HandlePointsUpdate);
    InPlayerState->OnKillsChanged.BindUObject(this, &UHamaMainWidget::HandleKillsUpdate);

    HandlePointsUpdate(InPlayerState->GetPoints());
    HandleKillsUpdate(InPlayerState->GetKills());

    if (PingText && !GetWorld()->GetTimerManager().IsTimerActive(PingUpdateTimer))
    {
        GetWorld()->GetTimerManager().SetTimer(PingUpdateTimer, this, &UHamaMainWidget::UpdatePingDisplay, 1.0f, true);
        UpdatePingDisplay();
    }
}

void UHamaMainWidget::BindGameState(ALastStandLegacyGameState* InGameState)
{
    if (!InGameState || bIsGameStateBound) return;

    InGameState->OnRoundChangedDelegate.AddUObject(this, &UHamaMainWidget::HandleRoundUpdate);
    HandleRoundUpdate(InGameState->GetCurrentRound());

    bIsGameStateBound = true;
}

// -------------------------------------------------------------------------
// Event Handlers
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
        static const FText InfinityText = FText::FromString(TEXT("\u221E / \u221E"));
        Ammo->SetText(InfinityText);

        if (AmmoWarningText) AmmoWarningText->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    FText FormattedAmmo = FText::Format(FText::FromString(TEXT("{0} / {1}")), CurrentAmmo, ReserveAmmo);
    Ammo->SetText(FormattedAmmo);

    if (AmmoWarningText && CachedHamaChar && CachedHamaChar->GetCurrentWeapon())
    {
        int32 MaxClipSize = CachedHamaChar->GetCurrentWeapon()->GetMaxClipAmmo();
        int32 LowAmmoThreshold = FMath::RoundToInt(MaxClipSize * 0.25f);

        if (CurrentAmmo == 0 && ReserveAmmo <= 0)
        {
            AmmoWarningText->SetText(FText::FromString(TEXT("No Ammo!")));
            AmmoWarningText->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else if (CurrentAmmo <= LowAmmoThreshold && CurrentAmmo > 0)
        {
            AmmoWarningText->SetText(FText::FromString(TEXT("LOW AMMO")));
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

void UHamaMainWidget::UpdatePingDisplay()
{
    if (!CachedHamaPS || !PingText) return;

    int32 PingValue = 0;

    if (GetOwningPlayer() && GetOwningPlayer()->HasAuthority())
    {
        PingValue = 0;
    }
    else
    {
        PingValue = FMath::RoundToInt(CachedHamaPS->GetPingInMilliseconds());
    }

    FText FormattedPing = FText::Format(FText::FromString(TEXT("{0} ms")), PingValue);
    PingText->SetText(FormattedPing);

    FSlateColor PingColor = FLinearColor::Green;

    if (PingValue > 150)
        PingColor = FLinearColor::Red;
    else if (PingValue > 80)
        PingColor = FLinearColor::Yellow;

    PingText->SetColorAndOpacity(PingColor);
}

void UHamaMainWidget::NativeDestruct()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PingUpdateTimer);
    }

    Super::NativeDestruct();
}