#pragma once

#include "CoreMinimal.h"
#include "GobbleGumAction.h"
#include "GobbleGumAction_DeadEyePrecision.generated.h"

class UGobbleGumComponent;

UCLASS()
class LASTSTANDLEGACY_API UGobbleGumAction_DeadEyePrecision : public UGobbleGumAction
{
    GENERATED_BODY()

public:
    UGobbleGumAction_DeadEyePrecision();

    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner) override;
    virtual void DeactivateAction_Implementation(ACharacter* CharacterOwner) override;

protected:
    TWeakObjectPtr<UGobbleGumComponent> CachedGumComp;
    FTimerHandle DurationTimerHandle;
};