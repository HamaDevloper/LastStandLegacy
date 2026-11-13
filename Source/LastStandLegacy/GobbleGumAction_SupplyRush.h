#pragma once

#include "CoreMinimal.h"
#include "GobbleGumAction.h"
#include "GobbleGumAction_SupplyRush.generated.h"

UCLASS(Blueprintable, meta = (DisplayName = "GobbleGum Action: Supply Rush"))
class LASTSTANDLEGACY_API UGobbleGumAction_SupplyRush : public UGobbleGumAction
{
    GENERATED_BODY()

public:
    UGobbleGumAction_SupplyRush();

    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner) override;
    virtual void DeactivateAction_Implementation(ACharacter* CharacterOwner) override;

private:
    TWeakObjectPtr<UWorld> CachedWorld;
};