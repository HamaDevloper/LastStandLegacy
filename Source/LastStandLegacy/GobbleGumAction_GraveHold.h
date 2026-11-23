#pragma once

#include "CoreMinimal.h"
#include "GobbleGumAction.h"
#include "GobbleGumAction_GraveHold.generated.h"

class ACharacter;
class UHealthComponent;

UCLASS(Blueprintable, EditInlineNew)
class LASTSTANDLEGACY_API UGobbleGumAction_GraveHold : public UGobbleGumAction
{
    GENERATED_BODY()

public:
    UGobbleGumAction_GraveHold();

    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner) override;
    virtual void DeactivateAction_Implementation(ACharacter* CharacterOwner) override;

protected:
    // چەند هێندە کاتی مانەوە لە دۆخی Downed درێژ بکاتەوە (2.0 = دوو هێندە، واتە 45 چرکە دەبێتە 90 چرکە)
    UPROPERTY(EditDefaultsOnly, Category = "GraveHold", meta = (ClampMin = "1.1", ClampMax = "10.0"))
    float BleedoutTimeMultiplier = 2.0f;

private:
    TWeakObjectPtr<UHealthComponent> CachedHealthComponent;
    bool bEffectApplied = false;
};