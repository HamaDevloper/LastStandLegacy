#pragma once

#include "CoreMinimal.h"
#include "GobbleGumAction.h"
#include "TimerManager.h"
#include "GobbleGumAction_MeleeBoost.generated.h"

class UGobbleGumComponent;

UCLASS(Blueprintable, meta = (DisplayName = "GobbleGum Action: Melee Boost"))
class LASTSTANDLEGACY_API UGobbleGumAction_MeleeBoost : public UGobbleGumAction
{
    GENERATED_BODY()

public:
    UGobbleGumAction_MeleeBoost();

    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner) override;
    virtual void DeactivateAction_Implementation(ACharacter* CharacterOwner) override;

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|MeleeBoost", meta = (ClampMin = "1.0"))
    float Duration = 45.0f;

private:
    TWeakObjectPtr<ACharacter> CachedOwner;
    TWeakObjectPtr<UGobbleGumComponent> CachedGumComponent;

    FTimerHandle TimerHandle_Duration;
    bool bEffectApplied = false;

    void OnDurationExpired();
};