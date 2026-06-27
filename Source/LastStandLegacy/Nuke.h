#pragma once

#include "CoreMinimal.h"
#include "BasePowerUp.h" 
#include "Nuke.generated.h"

UCLASS()
class LASTSTANDLEGACY_API ANuke : public ABasePowerUp
{
    GENERATED_BODY()

public:
    ANuke();

protected:
    virtual void ActivatePowerUp(AHama* Player) override;
};