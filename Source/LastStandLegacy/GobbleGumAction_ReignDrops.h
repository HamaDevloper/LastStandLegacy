#pragma once

#include "CoreMinimal.h"
#include "GobbleGumAction.h"
#include "GobbleGumAction_ReignDrops.generated.h"

class ABasePowerUp;

UCLASS(Blueprintable, meta = (DisplayName = "GobbleGum Action: Reign Drops"))
class LASTSTANDLEGACY_API UGobbleGumAction_ReignDrops : public UGobbleGumAction
{
    GENERATED_BODY()

public:
    UGobbleGumAction_ReignDrops();

    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner) override;

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|ReignDrops")
    TArray<TSubclassOf<ABasePowerUp>> PowerUpClasses;

    // دووریی سپۆنبوونی PowerUpـەکان لە یاریزانەوە (بە سانتیمەتر)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|ReignDrops", meta = (ClampMin = "0.0"))
    float SpawnDistance = 160.f;

    // گۆشەی بڵاوبوونەوەیان لە پێش یاریزاندا
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|ReignDrops", meta = (ClampMin = "0.0", ClampMax = "360.0"))
    float ArcAngleDegrees = 110.f;
};