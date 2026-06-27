#pragma once

#include "CoreMinimal.h"
#include "BasePowerUp.h"
#include "Instakill.generated.h"

UCLASS()
class LASTSTANDLEGACY_API AInstakill : public ABasePowerUp
{
    GENERATED_BODY()

public:
    AInstakill();

protected:
    virtual void ActivatePowerUp(AHama* Player) override;

    UPROPERTY(EditAnywhere, Category = "PowerUp")
    float Duration = 30.0f;
};