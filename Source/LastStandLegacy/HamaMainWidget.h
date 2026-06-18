#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "HamaMainWidget.generated.h"

UCLASS()
class LASTSTANDLEGACY_API UHamaMainWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // ئەم دوو فەنکشنە لە دەرەوە (لەناو کارەکتەر) بانگ دەکرێن
    void UpdatePointsText(int32 NewPoints);
    void UpdateKillsText(int32 NewKills);

protected:
    // مێتا بەیند: دەبێت لەناو بلوپرینتی شاشەکەتدا تێکستێک هەبێت ڕێک بە ناوی TXT_Points
    UPROPERTY(meta = (BindWidget))
    UTextBlock* Points;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* Kills;
};