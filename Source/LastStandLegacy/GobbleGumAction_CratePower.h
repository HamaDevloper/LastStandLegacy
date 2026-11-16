#pragma once

#include "CoreMinimal.h"
#include "GobbleGumAction.h"
#include "GobbleGumAction_CratePower.generated.h"

class UGobbleGumComponent;

/**
 * گۆبڵگەمی Crate Power: یەکەم چەک کە یاریزان لە Mystery Box هەڵیدەگرێتەوە بە شێوەی ئۆتۆماتیکی Pack-a-Punch دەبێت
 */
UCLASS(Blueprintable, meta = (DisplayName = "GobbleGum Action: Crate Power"))
class LASTSTANDLEGACY_API UGobbleGumAction_CratePower : public UGobbleGumAction
{
    GENERATED_BODY()

public:
    UGobbleGumAction_CratePower();

    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner) override;
    virtual void DeactivateAction_Implementation(ACharacter* CharacterOwner) override;

private:
    TWeakObjectPtr<UGobbleGumComponent> CachedGumComponent;
};