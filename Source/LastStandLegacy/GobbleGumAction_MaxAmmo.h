#pragma once

#include "CoreMinimal.h"
#include "GobbleGumAction.h"
#include "GobbleGumAction_MaxAmmo.generated.h"

class ABasePowerUp;

/**
 * گۆبڵگەمی Max Ammo (Cache Back): سپۆنکردنی PowerUpـی Max Ammo بۆ تەواوی تیمەکە
 */
UCLASS(Blueprintable, meta = (DisplayName = "GobbleGum Action: Max Ammo"))
class LASTSTANDLEGACY_API UGobbleGumAction_MaxAmmo : public UGobbleGumAction
{
    GENERATED_BODY()

public:
    UGobbleGumAction_MaxAmmo();

    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner) override;

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|MaxAmmo")
    TSubclassOf<ABasePowerUp> MaxAmmoPowerUpClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|MaxAmmo", meta = (ClampMin = "0.0"))
    float SpawnForwardDistance = 100.f;
};