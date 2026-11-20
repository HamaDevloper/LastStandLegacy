#pragma once

#include "CoreMinimal.h"
#include "GobbleGumAction.h"
#include "GobbleGumAction_MeleeBoost.generated.h"

class UGobbleGumComponent;

UCLASS(Blueprintable, meta = (DisplayName = "GobbleGum Action: Melee Insta-Kill"))
class LASTSTANDLEGACY_API UGobbleGumAction_MeleeBoost : public UGobbleGumAction
{
    GENERATED_BODY()

public:
    UGobbleGumAction_MeleeBoost();

    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner) override;
    virtual void DeactivateAction_Implementation(ACharacter* CharacterOwner) override;

private:
    TWeakObjectPtr<UGobbleGumComponent> CachedGumComponent;
};