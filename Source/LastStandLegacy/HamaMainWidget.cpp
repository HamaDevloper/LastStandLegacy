#include "HamaMainWidget.h"
#include "Hama.h"
#include "Components/TextBlock.h" // 🚀 هێدەرەکەمان هێنا بۆ ئێرە

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

void UHamaMainWidget::InitializeWidget(AHama* InHama)
{
    CachedHamaChar = InHama;
}

void UHamaMainWidget::UpdateAmmoText(int32 CurrentAmmo, int32 ReserveAmmo)
{
    if (!Ammo) return;

    if (CachedHamaChar && CachedHamaChar->GetDeathMachine())
    {
        // 🚀 بەکارهێنانی static بۆ ئەوەی CPU پشوو بدات لە کاتی تەقەکردنی خێرا
        static const FText InfinityText = FText::FromString(TEXT("\u221E / \u221E"));
        Ammo->SetText(InfinityText);
        return;
    }

    // 🚀 ڕێگای ڕاستەوخۆ (Direct Formatting) بەبێ بەکارهێنانی FString
    FText FormattedAmmo = FText::Format(FText::FromString(TEXT("{0} / {1}")), CurrentAmmo, ReserveAmmo);
    Ammo->SetText(FormattedAmmo);
}