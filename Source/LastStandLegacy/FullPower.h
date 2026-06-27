#pragma once

#include "CoreMinimal.h"
#include "BasePowerUp.h"
#include "FullPower.generated.h"

UCLASS()
class LASTSTANDLEGACY_API AFullPower : public ABasePowerUp
{
    GENERATED_BODY()

public:
    AFullPower();

protected:
    virtual void ActivatePowerUp(AHama* Player) override;
};