#pragma once

#include "CoreMinimal.h"
#include "BasePowerUp.h"
#include "DeathMachinePower.generated.h"

class ABaseWeapon;

UCLASS()
class LASTSTANDLEGACY_API ADeathMachinePower : public ABasePowerUp
{
    GENERATED_BODY()

protected:
    virtual void ActivatePowerUp(AHama* Player) override;

    UPROPERTY(EditAnywhere, Category = "PowerUp Settings")
    TSubclassOf<ABaseWeapon> MachineGunClass;

    UPROPERTY(EditAnywhere, Category = "PowerUp Settings")
    float Duration = 30.0f;
};