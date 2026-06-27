#pragma once

#include "CoreMinimal.h"
#include "BasePowerUp.h"
#include "DoublePoint.generated.h"

UCLASS()
class LASTSTANDLEGACY_API ADoublePoint : public ABasePowerUp
{
    GENERATED_BODY()

public:
    ADoublePoint();

protected:
    virtual void ActivatePowerUp(AHama* Player) override;

    UPROPERTY(EditAnywhere, Category = "PowerUp")
    float Duration = 30.0f;
};