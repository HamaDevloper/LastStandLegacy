#pragma once

#include "CoreMinimal.h"
#include "BasePowerUp.h"
#include "BonusPoints.generated.h"

UCLASS()
class LASTSTANDLEGACY_API ABonusPoints : public ABasePowerUp
{
    GENERATED_BODY()

public:
    ABonusPoints();

protected:
    virtual void ActivatePowerUp(AHama* Player) override;

    UPROPERTY(EditAnywhere, Category = "PowerUp")
    float AddPoints = 1000.f;
};