#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GobbleGumTypes.h"
#include "GobbleGumData.generated.h"

class UGobbleGumAction;
class UTexture2D;

/**
 * داتای جێگیری هەر گۆبڵگەمێک کە لەناو وێدەگۆڕ (Content Browser) دروست دەکرێت
 */
UCLASS(BlueprintType)
class LASTSTANDLEGACY_API UGobbleGumData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ناوی گەمەکە بۆ پیشاندان لەسەر شاشە و UI
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
    FText GumName;

    // وەسفی کورت لەسەر کاریگەری گەمەکە
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display", meta = (MultiLine = true))
    FText Description;

    // ئایکۆنی ٢D بۆ ناو HUD
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
    TObjectPtr<UTexture2D> Icon;

    // ڕەنگ و دەگمەنی (Classic, Mega, هتد)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
    EGobbleGumRarity Rarity = EGobbleGumRarity::Classic;

    // شێوازی کارکردن (دەستبەجێ، بە دوگمە، خولاوخول)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mechanics")
    EGobbleGumActivationType ActivationType = EGobbleGumActivationType::Immediate;

    // چەند جار دەتوانرێت بەکاربهێنرێت پێش ئەوەی تەواو بێت (بۆ ئەوانەی بە دوگمە کاردەکەن)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mechanics", meta = (ClampMin = "1", UIMin = "1"))
    uint8 MaxUses = 1;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mechanics", meta = (ClampMin = "1", UIMin = "1", EditCondition = "bOverrideUsesInSolo", EditConditionHides))
    uint8 SoloMaxUses = 2;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mechanics")
    bool bOverrideUsesInSolo = false;

    // ئەگەر جۆرەکەی RoundBased بێت، چەند خول کار دەکات
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mechanics", meta = (EditCondition = "ActivationType == EGobbleGumActivationType::RoundBased", EditConditionHides))
    uint8 DurationRounds = 3;

    // لۆژیکی کاریگەرییەکە - بەکارهێنانی Instanced ڕێگە دەدات ڕاستەوخۆ لەناو ئەم داتا ئەسێتەدا لۆژیک دیاری بکەیت
    UPROPERTY(EditDefaultsOnly, Instanced, Category = "Logic")
    TObjectPtr<UGobbleGumAction> ActionLogic;
};