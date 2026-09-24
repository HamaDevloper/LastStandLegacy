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

    if (GrenadeContainer) GrenadeContainer->ClearChildren();
    if (MonkeyContainer) MonkeyContainer->ClearChildren();

    // ---------------------------------------------------------
    // Perk Pool Initialization (ئەمەیان لە UI بڕەکەی جێگیرە)
    // ---------------------------------------------------------
    if (PerkContainer && PerkImagePool.Num() == 0)
    {
        PerkContainer->ClearChildren();

        for (int32 i = 0; i < MaxPerkSlots; ++i)
        {
            UImage* NewPerkImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
            if (NewPerkImage)
            {
                NewPerkImage->SetVisibility(ESlateVisibility::Collapsed);

                if (UHorizontalBoxSlot* PerkSlot = PerkContainer->AddChildToHorizontalBox(NewPerkImage))
                {
                    PerkSlot->SetPadding(FMargin(4.0f, 0.0f, 4.0f, 0.0f));
                    PerkSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
                    PerkSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Center);
                    PerkSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);
                }

                PerkImagePool.Add(NewPerkImage);
            }
        }
    }
}

void UHamaMainWidget::BindCharacter(AHama* InHama)
{
    if (!InHama || CachedHamaChar == InHama) return;

    if (CachedHamaChar)
    {
        if (UThrowableComponent* OldThrowableComp = CachedHamaChar->FindComponentByClass<UThrowableComponent>())
        {
            OldThrowableComp->OnThrowableCountChanged.Unbind();
        }

        CachedHamaChar->OnAmmoUpdateEvent.Unbind();
        CachedHamaChar->OnInteractUpdateEvent.Unbind();
        CachedHamaChar->OnCrosshairUpdateEvent.Unbind();
        CachedHamaChar->OnPerksChangedEvent.Unbind();
        CachedHamaChar->OnPersonalPowerUpAcquiredDelegate.Unbind();
    }

    CachedHamaChar = InHama;

    CachedHamaChar->OnAmmoUpdateEvent.BindUObject(this, &UHamaMainWidget::HandleAmmoUpdate);
    CachedHamaChar->OnInteractUpdateEvent.BindUObject(this, &UHamaMainWidget::HandleInteractUpdate);
    CachedHamaChar->OnCrosshairUpdateEvent.BindUObject(this, &UHamaMainWidget::HandleCrosshairUpdate);
    CachedHamaChar->OnPerksChangedEvent.BindUObject(this, &UHamaMainWidget::HandlePerksUpdate);
    CachedHamaChar->OnPersonalPowerUpAcquiredDelegate.BindUObject(this, &UHamaMainWidget::ShowPowerMessage);

    if (UThrowableComponent* ThrowableComp = CachedHamaChar->FindComponentByClass<UThrowableComponent>())
    {
        ThrowableComp->OnThrowableCountChanged.BindUObject(this, &UHamaMainWidget::HandleThrowableCountUpdate);

        EnsureThrowablePoolSize(GrenadeContainer, GrenadeImagePool, GrenadeIconTexture, ThrowableComp->GetMaxGrenadeCount());
        EnsureThrowablePoolSize(MonkeyContainer, MonkeyImagePool, MonkeyIconTexture, ThrowableComp->GetMaxMonkeyCount());

        HandleThrowableCountUpdate(ThrowableComp->GetCurrentMonkeyCount(), ThrowableComp->GetCurrentGrenadeCount());
    }

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

static const FNumberFormattingOptions NoGroupingOptions = FNumberFormattingOptions().SetUseGrouping(false);

void UHamaMainWidget::HandlePointsUpdate(int32 NewPoints)
{
    if (Points)
    {
        static const FText PointsFormatPattern = LOCTEXT("PointsFormat", "${ 0}");
        const FText FormattedNumber = FText::AsNumber(NewPoints, &NoGroupingOptions);
        Points->SetText(FText::Format(PointsFormatPattern, FormattedNumber));
    }
}

void UHamaMainWidget::HandleKillsUpdate(int32 NewKills)
{
    if (Kills)
    {
        Kills->SetText(FText::AsNumber(NewKills, &NoGroupingOptions));
    }
}

void UHamaMainWidget::HandleRoundUpdate(int32 NewRound)
{
    if (Round)
    {
        Round->SetText(FText::AsNumber(NewRound, &NoGroupingOptions));
    }
}

void UHamaMainWidget::HandleAmmoUpdate(int32 CurrentAmmo, int32 ReserveAmmo)
{
    if (!Ammo) return;

    if (!CachedHamaChar || !CachedHamaChar->GetCurrentWeapon())
    {
        GEngine->AddOnScreenDebugMessage(1, 2.f, FColor::Green, "No Weapon Avalible");
        Ammo->SetText(FText::GetEmpty());
        if (AmmoWarningText)
        {
            AmmoWarningText->SetVisibility(ESlateVisibility::Hidden);
        }
        return;
    }

    if (CachedHamaChar->GetDeathMachine())
    {
        static const FText InfinityText = LOCTEXT("InfinityAmmo", "\u221E / \u221E");
        Ammo->SetText(InfinityText);

        if (AmmoWarningText) AmmoWarningText->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    const FText CurrentText = FText::AsNumber(CurrentAmmo, &NoGroupingOptions);
    const FText ReserveText = FText::AsNumber(ReserveAmmo, &NoGroupingOptions);

    static const FText AmmoFormatPattern = LOCTEXT("AmmoFormat", "{0} / {1}");
    Ammo->SetText(FText::Format(AmmoFormatPattern, CurrentText, ReserveText));

    ABaseWeapon* CurrentWeapon = CachedHamaChar->GetCurrentWeapon();
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

    const FText FormattedPing = FText::AsNumber(PingValue, &NoGroupingOptions);
    static const FText PingFormat = LOCTEXT("PingFormat", "{0} ms");
    PingText->SetText(FText::Format(PingFormat, FormattedPing));

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
    if (!PerkContainer || PerkImagePool.Num() == 0) return;

    const FVector2D DesiredPerkSize(Perksize, Perksize);
    const int32 NumOwnedPerks = CurrentPerks.Num();

    for (int32 i = 0; i < PerkImagePool.Num(); ++i)
    {
        UImage* PerkSlotImage = PerkImagePool[i];
        if (!PerkSlotImage) continue;

        if (i < NumOwnedPerks)
        {
            const FName& PerkID = CurrentPerks[NumOwnedPerks - 1 - i];

            if (const TObjectPtr<UTexture2D>* FoundTexture = PerkIcons.Find(PerkID))
            {
                if (*FoundTexture)
                {
                    FSlateBrush PerkBrush;
                    PerkBrush.SetResourceObject(*FoundTexture);
                    PerkBrush.ImageSize = DesiredPerkSize;

                    PerkSlotImage->SetBrush(PerkBrush);
                    PerkSlotImage->SetDesiredSizeOverride(DesiredPerkSize);
                    PerkSlotImage->SetVisibility(ESlateVisibility::HitTestInvisible);
                    continue;
                }
            }
        }

        PerkSlotImage->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UHamaMainWidget::EnsureThrowablePoolSize(UHorizontalBox* Container, TArray<TObjectPtr<UImage>>& Pool, UTexture2D* IconTexture, int32 TargetSize)
{
    if (!Container || TargetSize <= 0) return;

    while (Pool.Num() < TargetSize)
    {
        UImage* NewImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        if (!NewImage) break;

        NewImage->SetVisibility(ESlateVisibility::Collapsed);

        if (IconTexture)
        {
            FSlateBrush Brush;
            Brush.SetResourceObject(IconTexture);
            Brush.ImageSize = ThrowableIconSize;
            NewImage->SetBrush(Brush);
        }

        if (UHorizontalBoxSlot* NewSlot = Container->AddChildToHorizontalBox(NewImage))
        {
            NewSlot->SetPadding(FMargin(2.0f, 0.0f, 2.0f, 0.0f));
            NewSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
            NewSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Center);
        }

        Pool.Add(NewImage);
    }
}


void UHamaMainWidget::HandleThrowableCountUpdate(int32 MonkeyCount, int32 GrenadeCount)
{
    // -------------------------------------------------------------------------
    // 1. Grenade Icons Display
    // -------------------------------------------------------------------------
    const int32 TotalGrenades = GrenadeImagePool.Num();
    for (int32 i = 0; i < TotalGrenades; ++i)
    {
        if (UImage* GrenadeImg = GrenadeImagePool[i])
        {
            const bool bShouldShow = (i >= (TotalGrenades - GrenadeCount));
            GrenadeImg->SetVisibility(bShouldShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
        }
    }

    // -------------------------------------------------------------------------
    // 2. Monkey Bomb Icons Display
    // -------------------------------------------------------------------------
    const int32 TotalMonkeys = MonkeyImagePool.Num();
    for (int32 i = 0; i < TotalMonkeys; ++i)
    {
        if (UImage* MonkeyImg = MonkeyImagePool[i])
        {
            const bool bShouldShow = (i >= (TotalMonkeys - MonkeyCount));
            MonkeyImg->SetVisibility(bShouldShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
        }
    }
}

void UHamaMainWidget::UnbindAllEvents()
{
    if (CachedHamaChar)
    {
        if (UThrowableComponent* ThrowableComp = CachedHamaChar->FindComponentByClass<UThrowableComponent>())
        {
            ThrowableComp->OnThrowableCountChanged.Unbind();
        }

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