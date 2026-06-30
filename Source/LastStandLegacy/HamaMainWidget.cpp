#include "HamaMainWidget.h"
#include "Hama.h"

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
        Ammo->SetText(FText::FromString(TEXT("\u221E / \u221E")));
        return;
    }

    FString AmmoString = FString::Printf(TEXT("%d / %d"), CurrentAmmo, ReserveAmmo);
    Ammo->SetText(FText::FromString(AmmoString));
}