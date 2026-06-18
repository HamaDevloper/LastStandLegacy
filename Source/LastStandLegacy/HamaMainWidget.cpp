#include "HamaMainWidget.h"

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