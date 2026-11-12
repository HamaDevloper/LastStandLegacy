#pragma once

#include "CoreMinimal.h"
#include "GobbleGumAction.h"
#include "UGobbleGumAction_Perkaholic.generated.h"

/**
 * گۆبڵگەمی Perkaholic: پێدانی هەموو پێرکەکانی ناو مەپەکە بە یاریزان بە شێوەی دەستبەجێ
 */
UCLASS(Blueprintable, meta = (DisplayName = "GobbleGum Action: Perkaholic"))
class LASTSTANDLEGACY_API UGobbleGumAction_Perkaholic : public UGobbleGumAction
{
    GENERATED_BODY()

public:
    UGobbleGumAction_Perkaholic();

    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner) override;

protected:
    // ئەگەر ئەم لیستە بەتاڵ بێت، خۆکارانە هەموو پێرکەکانی ناو مەپەکە (ABasePerk) دەداتە یاریزانەکە.
    // ئەگەریش ناوی پێرکەکانی تێدا بنووسیت، تەنها ئەوانەی ناو ئەم لیستە دەداتێ.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perkaholic")
    TArray<FName> CustomPerksToGrant;
};