#pragma once

#include "CoreMinimal.h"
#include "GobbleGumAction.h"
#include "GobbleGumAction_SpawnPowerUp.generated.h"

class ABasePowerUp;

/**
 * کلاسی گشتی بۆ هەر گۆبڵگەمێک کە یەک PowerUp لە بەردەم یاریزان سپۆن دەکات
 */
UCLASS(Blueprintable, meta = (DisplayName = "GobbleGum Action: Spawn PowerUp"))
class LASTSTANDLEGACY_API UGobbleGumAction_SpawnPowerUp : public UGobbleGumAction
{
    GENERATED_BODY()

public:
    UGobbleGumAction_SpawnPowerUp();

    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner) override;

protected:
    // بلوپرینتی ئەو PowerUpـەی دەتەوێت سپۆن ببێت (MaxAmmo, InstaKill, DoublePoints, ...)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|PowerUp")
    TSubclassOf<ABasePowerUp> PowerUpClass;

    // دووریی سپۆنکردن لە بەردەم یاریزانەکە (بە سانتیمەتر)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|PowerUp", meta = (ClampMin = "0.0"))
    float SpawnForwardDistance = 100.f;
};