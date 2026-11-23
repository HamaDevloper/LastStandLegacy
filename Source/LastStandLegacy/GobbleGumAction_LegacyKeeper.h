#pragma once

#include "CoreMinimal.h"
#include "GobbleGumAction.h"
#include "GobbleGumAction_LegacyKeeper.generated.h"

class ACharacter;
class AHamaPlayerState;

UCLASS(Blueprintable, EditInlineNew)
class LASTSTANDLEGACY_API UGobbleGumAction_LegacyKeeper : public UGobbleGumAction
{
    GENERATED_BODY()

public:
    UGobbleGumAction_LegacyKeeper();

    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner) override;
    virtual void DeactivateAction_Implementation(ACharacter* CharacterOwner) override;

private:
    TWeakObjectPtr<AHamaPlayerState> CachedPlayerState;
    bool bArmedByThisAction = false;
};