#pragma once

#include "CoreMinimal.h"
#include "GobbleGumAction.h"
#include "GobbleGumAction_CopperMagnet.generated.h"

class ACharacter;
class UGobbleGumComponent;

UCLASS(Blueprintable, EditInlineNew)
class LASTSTANDLEGACY_API UGobbleGumAction_CopperMagnet : public UGobbleGumAction
{
    GENERATED_BODY()

public:
    UGobbleGumAction_CopperMagnet();

    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner) override;
    virtual void DeactivateAction_Implementation(ACharacter* CharacterOwner) override;

protected:
    // ڕێژەی زیادکردنی پۆینت (1.25 = +25% پۆینتی زیادە)
    UPROPERTY(EditDefaultsOnly, Category = "CopperMagnet", meta = (ClampMin = "1.05", ClampMax = "5.0"))
    float Multiplier = 1.25f;

private:
    TWeakObjectPtr<UGobbleGumComponent> CachedGumComponent;
    bool bEffectApplied = false;
};