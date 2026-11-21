#pragma once

#include "CoreMinimal.h"
#include "GobbleGumAction.h"
#include "TimerManager.h"
#include "GobbleGumAction_VolatileCore.generated.h"

class UThrowableComponent;
class UGobbleGumComponent;
class AGrenade;

/**
 * Volatile Core: لە ماوەی چالاکبوونیدا، هەر نارنجۆکێک فڕێ بدرێت دەبێتە
 * Insta-Kill و لە هەر خولێکدا بێت زۆمبییەکانی ناو تەقینەوەکە یەکسەر دەکوژێت.
 */
UCLASS(Blueprintable, meta = (DisplayName = "GobbleGum Action: Volatile Core"))
class LASTSTANDLEGACY_API UGobbleGumAction_VolatileCore : public UGobbleGumAction
{
    GENERATED_BODY()

public:
    UGobbleGumAction_VolatileCore();

    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner) override;
    virtual void DeactivateAction_Implementation(ACharacter* CharacterOwner) override;

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|VolatileCore", meta = (ClampMin = "1.0"))
    float Duration = 45.0f;

private:
    TWeakObjectPtr<ACharacter> CachedOwner;
    TWeakObjectPtr<UThrowableComponent> CachedThrowableComponent;
    TWeakObjectPtr<UGobbleGumComponent> CachedGumComponent;

    FTimerHandle TimerHandle_Duration;
    FDelegateHandle GrenadeThrownHandle;

    bool bEffectApplied = false;

    void HandleGrenadeThrown(AGrenade* ThrownGrenade);
    void OnDurationExpired();
};