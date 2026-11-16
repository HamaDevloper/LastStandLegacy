#pragma once

#include "CoreMinimal.h"
#include "GobbleGumAction.h"
#include "GobbleGumAction_WallPower.generated.h"

class UGobbleGumComponent;

/**
 * گۆبڵگەمی Wall Power: یەکەم چەکی سەر دیوار کە یاریزان دەیکڕێت بە شێوەی ئۆتۆماتیکی Pack-a-Punch دەبێت
 */
UCLASS(Blueprintable, meta = (DisplayName = "GobbleGum Action: Wall Power"))
class LASTSTANDLEGACY_API UGobbleGumAction_WallPower : public UGobbleGumAction
{
    GENERATED_BODY()

public:
    UGobbleGumAction_WallPower();

    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner) override;
    virtual void DeactivateAction_Implementation(ACharacter* CharacterOwner) override;

private:
    TWeakObjectPtr<UGobbleGumComponent> CachedGumComponent;
};