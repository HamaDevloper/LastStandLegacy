#include "HamaMainWidget.h"
#include "Hama.h"
#include "LastStandLegacyGameState.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "HamaPlayerState.h"
#include "BaseWeapon.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Blueprint/WidgetTree.h"

#define LOCTEXT_NAMESPACE "HamaMainWidget"

// -------------------------------------------------------------------------
// Initialization & Bindings
// -------------------------------------------------------------------------

void UHamaMainWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (PowerImage)
    {
        PowerImage->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (PowerUpAnim)
    {
        PowerUpAnimDelegate.BindDynamic(this, &UHamaMainWidget::OnPowerUpAnimFinished);
    }
}

void UHamaMainWidget::BindCharacter(AHama* InHama)
{
    if (!InHama || CachedHamaChar == InHama) return;

    if (CachedHamaChar)
    {
        CachedHamaChar->OnAmmoUpdateEvent.Unbind();
        CachedHamaChar->OnInteractUpdateEvent.Unbind();
        CachedHamaChar->OnCrosshairUpdateEvent.Unbind();
        CachedHamaChar->OnPerksChangedEvent.Unbind();
    }

    CachedHamaChar = InHama;

    CachedHamaChar->OnAmmoUpdateEvent.BindUObject(this, &UHamaMainWidget::HandleAmmoUpdate);
    CachedHamaChar->OnInteractUpdateEvent.BindUObject(this, &UHamaMainWidget::HandleInteractUpdate);
    CachedHamaChar->OnCrosshairUpdateEvent.BindUObject(this, &UHamaMainWidget::HandleCrosshairUpdate);
    CachedHamaChar->OnPerksChangedEvent.BindUObject(this, &UHamaMainWidget::HandlePerksUpdate);

    HandlePerksUpdate(CachedHamaChar->GetOwnedPerks());

    if (ABaseWeapon* CurrentWep = CachedHamaChar->GetCurrentWeapon())
    {
        HandleAmmoUpdate(CurrentWep->GetCurrentAmmo(), CurrentWep->GetReserveAmmo());
    }
}

void UHamaMainWidget::BindPlayerState(AHamaPlayerState* InPlayerState)
{
    if (!InPlayerState || CachedHamaPS == InPlayerState) return;

    if (CachedHamaPS)
    {
        CachedHamaPS->OnPointsChanged.Unbind();
        CachedHamaPS->OnKillsChanged.Unbind();
    }

    CachedHamaPS = InPlayerState;

    CachedHamaPS->OnPointsChanged.BindUObject(this, &UHamaMainWidget::HandlePointsUpdate);
    CachedHamaPS->OnKillsChanged.BindUObject(this, &UHamaMainWidget::HandleKillsUpdate);

    HandlePointsUpdate(CachedHamaPS->GetPoints());
    HandleKillsUpdate(CachedHamaPS->GetKills());

    if (PingText && GetWorld() && !GetWorld()->GetTimerManager().IsTimerActive(PingUpdateTimer))
    {
        GetWorld()->GetTimerManager().SetTimer(PingUpdateTimer, this, &UHamaMainWidget::UpdatePingDisplay, 1.0f, true);
        UpdatePingDisplay();
    }
}

void UHamaMainWidget::BindGameState(ALastStandLegacyGameState* InGameState)
{
    if (!InGameState || CachedGameState == InGameState) return;

    if (CachedGameState)
    {
        CachedGameState->OnRoundChangedDelegate.RemoveAll(this);
        CachedGameState->OnPowerUpAnnouncedDelegate.RemoveAll(this);
    }

    CachedGameState = InGameState;
    CachedGameState->OnRoundChangedDelegate.AddUObject(this, &UHamaMainWidget::HandleRoundUpdate);
    CachedGameState->OnPowerUpAnnouncedDelegate.AddUObject(this, &UHamaMainWidget::ShowPowerMessage);

    HandleRoundUpdate(CachedGameState->GetCurrentRound());
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
        static const FText InfinityText = LOCTEXT("InfinityAmmo", "\u221E / \u221E");
        Ammo->SetText(InfinityText);

        if (AmmoWarningText) AmmoWarningText->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    static const FText AmmoFormatPattern = LOCTEXT("AmmoFormat", "{0} / {1}");
    Ammo->SetText(FText::Format(AmmoFormatPattern, CurrentAmmo, ReserveAmmo));

    ABaseWeapon* CurrentWeapon = CachedHamaChar ? CachedHamaChar->GetCurrentWeapon() : nullptr;

    if (AmmoWarningText && CurrentWeapon)
    {
        int32 MaxClipSize = CurrentWeapon->GetMaxClipAmmo();
        int32 LowAmmoThreshold = FMath::RoundToInt(MaxClipSize * 0.25f);

        if (CurrentAmmo == 0 && ReserveAmmo <= 0)
        {
            static const FText NoAmmoText = LOCTEXT("NoAmmo", "NO AMMO");
            AmmoWarningText->SetText(NoAmmoText);
            AmmoWarningText->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else if (CurrentAmmo == 0 && ReserveAmmo > 0)
        {
            static const FText ReloadText = LOCTEXT("ReloadAmmo", "RELOAD");
            AmmoWarningText->SetText(ReloadText);
            AmmoWarningText->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else if (CurrentAmmo <= LowAmmoThreshold)
        {
            static const FText LowAmmoText = LOCTEXT("LowAmmo", "LOW AMMO!");
            AmmoWarningText->SetText(LowAmmoText);
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
    if (!InteractText) return;

    if (Message.IsEmpty())
    {
        InteractText->SetVisibility(ESlateVisibility::Collapsed);
    }
    else
    {
        InteractText->SetText(FText::FromString(Message));
        InteractText->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
}

void UHamaMainWidget::HandleCrosshairUpdate(bool bIsAimingAtEnemy)
{
    if (CrosshairImage)
    {
        CrosshairImage->SetColorAndOpacity(bIsAimingAtEnemy ? FLinearColor::Red : FLinearColor::White);
    }
}

void UHamaMainWidget::UpdatePingDisplay()
{
    if (!CachedHamaPS || !PingText) return;

    int32 PingValue = 0;

    PingValue = FMath::RoundToInt(CachedHamaPS->GetPingInMilliseconds());

    static const FText PingFormat = LOCTEXT("PingFormat", "{0} ms");
    PingText->SetText(FText::Format(PingFormat, PingValue));

    FSlateColor PingColor = FLinearColor::Green;
    if (PingValue > 150)
        PingColor = FLinearColor::Red;
    else if (PingValue > 80)
        PingColor = FLinearColor::Yellow;

    PingText->SetColorAndOpacity(PingColor);
}

void UHamaMainWidget::ShowPowerMessage(EPowerUpType PowerUpType)
{
    if (!PowerImage || !PowerUpAnim) return;

    const TObjectPtr<UTexture2D>* FoundIcon = PowerUpIcons.Find(PowerUpType);
    if (!FoundIcon || !(*FoundIcon)) return;

    if (IsAnimationPlaying(PowerUpAnim))
    {
        UnbindFromAnimationFinished(PowerUpAnim, PowerUpAnimDelegate);
        StopAnimation(PowerUpAnim);
    }

    PowerImage->SetBrushFromTexture(*FoundIcon);
    PowerImage->SetVisibility(ESlateVisibility::HitTestInvisible);

    BindToAnimationFinished(PowerUpAnim, PowerUpAnimDelegate);
    PlayAnimation(PowerUpAnim, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f, false);
}

void UHamaMainWidget::OnPowerUpAnimFinished()
{
    if (PowerUpAnim)
    {
        UnbindFromAnimationFinished(PowerUpAnim, PowerUpAnimDelegate);
    }

    if (PowerImage)
    {
        PowerImage->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UHamaMainWidget::HandlePerksUpdate(const TArray<FName>& CurrentPerks)
{
    if (!PerkContainer) return;

    PerkContainer->ClearChildren();

    const FVector2D DesiredPerkSize(Perksize, Perksize);

    for (const FName& PerkID : CurrentPerks)
    {
        if (const TObjectPtr<UTexture2D>* FoundTexture = PerkIcons.Find(PerkID))
        {
            if (*FoundTexture)
            {
                UImage* NewPerkImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
                if (NewPerkImage)
                {
                    FSlateBrush PerkBrush;
                    PerkBrush.SetResourceObject(*FoundTexture);
                    PerkBrush.ImageSize = DesiredPerkSize;

                    NewPerkImage->SetBrush(PerkBrush);
                    NewPerkImage->SetDesiredSizeOverride(DesiredPerkSize);

                    if (UHorizontalBoxSlot* PerkSlot = PerkContainer->AddChildToHorizontalBox(NewPerkImage))
                    {
                        PerkSlot->SetPadding(FMargin(4.0f, 0.0f, 4.0f, 0.0f));
                        PerkSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
                        PerkSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Center);
                        PerkSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);
                    }
                }
            }
        }
    }
}

void UHamaMainWidget::UnbindAllEvents()
{
    if (CachedHamaChar)
    {
        CachedHamaChar->OnAmmoUpdateEvent.Unbind();
        CachedHamaChar->OnInteractUpdateEvent.Unbind();
        CachedHamaChar->OnCrosshairUpdateEvent.Unbind();
        CachedHamaChar->OnPerksChangedEvent.Unbind();
        CachedHamaChar = nullptr;
    }

    if (CachedHamaPS)
    {
        CachedHamaPS->OnPointsChanged.Unbind();
        CachedHamaPS->OnKillsChanged.Unbind();
        CachedHamaPS = nullptr;
    }

    if (CachedGameState)
    {
        CachedGameState->OnRoundChangedDelegate.RemoveAll(this);
        CachedGameState->OnPowerUpAnnouncedDelegate.RemoveAll(this);
        CachedGameState = nullptr;
    }
}

void UHamaMainWidget::NativeDestruct()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PingUpdateTimer);
    }

    UnbindAllEvents();

    Super::NativeDestruct();
}

#undef LOCTEXT_NAMESPACE