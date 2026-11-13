#pragma once

#include "CoreMinimal.h"
#include "GobbleGumAction.h"
#include "GobbleGumAction_BonusPoints.generated.h"

class ABasePowerUp;
/**
 * گۆبڵگەمی پۆینت (Extra Credit / Bonus Points): پێدانی پارە بە یاریزان یان تەواوی تیمەکە
 */
UCLASS(Blueprintable, meta = (DisplayName = "GobbleGum Action: Bonus Points"))
class LASTSTANDLEGACY_API UGobbleGumAction_BonusPoints : public UGobbleGumAction
{
    GENERATED_BODY()

public:
    UGobbleGumAction_BonusPoints();

    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner) override;

protected:

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|points")
    TSubclassOf<ABasePowerUp> PointsClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|MaxAmmo", meta = (ClampMin = "0.0"))
    float SpawnForwardDistance = 100.f;
};