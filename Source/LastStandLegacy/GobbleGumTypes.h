#pragma once

#include "CoreMinimal.h"
#include "GobbleGumTypes.generated.h"

/**
 * شێوازی چالاکبوونی گەمەکە لەناو یارییەکەدا
 */
UENUM(BlueprintType)
enum class EGobbleGumActivationType : uint8
{
    // دەستبەجێ کاتێک گەمەکە لە دەم دەدرێت کاردەکات (وەک: In Plain Sight ئەگەر دەستبەجێ بێت، یان پۆینتی کاش)
    Immediate       UMETA(DisplayName = "Immediate"),

    // یاریزان بە دوگمەی دیاریکراو (وەک Up D-Pad یان دوگمەی تایبەت) بە ئارەزووی خۆی چالاکی دەکات
    Activated       UMETA(DisplayName = "Activated (Manual)"),

    // بەپێی خول (Round) کاردەکات و لەگەڵ تەواوبوونی خولەکان کەم دەبێتەوە
    RoundBased      UMETA(DisplayName = "Round Based"),

    // لەسەر ڕووداوێکی دیاریکراو کاردەکات (وەک لێدان لە زۆمبی، ڕزگارکردنی هاوڕێ، کەوتن)
    EventTriggered  UMETA(DisplayName = "Event Triggered")
};

/**
 * ڕەنگ و پلەی دەگمەنی گەمەکە (تەنها بۆ UI و ڕووناکییەکانی دەوری ئامێرەکە)
 */
UENUM(BlueprintType)
enum class EGobbleGumRarity : uint8
{
    Classic         UMETA(DisplayName = "Classic (Blue)"),
    Mega            UMETA(DisplayName = "Mega (Purple)"),
    RareMega        UMETA(DisplayName = "Rare Mega (Green)"),
    UltraRareMega   UMETA(DisplayName = "Ultra Rare Mega (Orange)")
};