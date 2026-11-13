#pragma once

#include "CoreMinimal.h"
#include "GobbleGumAction.h"
#include "GobbleGumAction_PerkSaver.generated.h"

class UGobbleGumComponent;

/**
 * گۆبڵگەمی PerkSaver (Aftertaste): پاراستنی پێرکەکانی یاریزان کاتێک دەکەوێتە سەر زەوی (Down)
 */
UCLASS(Blueprintable, meta = (DisplayName = "GobbleGum Action: Perk Saver"))
class LASTSTANDLEGACY_API UGobbleGumAction_PerkSaver : public UGobbleGumAction
{
    GENERATED_BODY()

public:
    UGobbleGumAction_PerkSaver();

    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner) override;
    virtual void DeactivateAction_Implementation(ACharacter* CharacterOwner) override;

private:
    TWeakObjectPtr<UGobbleGumComponent> CachedGumComponent;
};