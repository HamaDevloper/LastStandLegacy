#pragma once

#include "CoreMinimal.h"
#include "GobbleGumAction.h"
#include "GobbleGumAction_ReignDrops.generated.h"

class ABasePowerUp;

/**
 * گۆبڵگەمی Reign Drops: سپۆنکردنی هەموو جۆرەکانی PowerUp پێکەوە بە شێوەی کەوانەیی لە بەردەم یاریزاندا
 */
UCLASS(Blueprintable, meta = (DisplayName = "GobbleGum Action: Reign Drops"))
class LASTSTANDLEGACY_API UGobbleGumAction_ReignDrops : public UGobbleGumAction
{
    GENERATED_BODY()

public:
    UGobbleGumAction_ReignDrops();

    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner) override;

protected:
    // لیستی هەموو ئەو PowerUpـانەی دەتەوێت پێکەوە بێنە خوارەوە
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|ReignDrops")
    TArray<TSubclassOf<ABasePowerUp>> PowerUpClasses;

    // دووریی کەوانەکە لە یاریزانەکەوە (بە سانتیمەتر)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|ReignDrops", meta = (ClampMin = "60.0"))
    float SpawnRadius = 160.f;

    // گۆشەی کرانەوەی کەوانەکە لە بەردەم یاریزاندا (بۆ نموونە 110 پلە)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|ReignDrops", meta = (ClampMin = "10.0", ClampMax = "360.0"))
    float ArcAngleDegrees = 110.f;
};