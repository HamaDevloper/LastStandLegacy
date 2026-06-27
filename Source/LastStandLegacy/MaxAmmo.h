#pragma once

#include "CoreMinimal.h"
#include "BasePowerUp.h"
#include "MaxAmmo.generated.h"

UCLASS()
class LASTSTANDLEGACY_API AMaxAmmo : public ABasePowerUp
{
    GENERATED_BODY()

public:
    AMaxAmmo();

protected:
    virtual void ActivatePowerUp(AHama* Player) override;
};