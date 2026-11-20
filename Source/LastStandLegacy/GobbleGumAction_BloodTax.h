#pragma once

#include "CoreMinimal.h"
#include "GobbleGumAction.h"
#include "TimerManager.h"
#include "GobbleGumAction_BloodTax.generated.h"

class UGobbleGumComponent;

UCLASS(Blueprintable, meta = (DisplayName = "GobbleGum Action: Blood Tax"))
class LASTSTANDLEGACY_API UGobbleGumAction_BloodTax : public UGobbleGumAction
{
    GENERATED_BODY()

public:
    UGobbleGumAction_BloodTax();

    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner) override;
    virtual void DeactivateAction_Implementation(ACharacter* CharacterOwner) override;

    void ForceExpireFromBankruptcy();

protected:
    // ڕێژەی زیادبوونی خاڵ (2.0 واتە دوو هێندە)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|BloodTax", meta = (ClampMin = "1.0"))
    float ScoreMultiplier = 2.0f;

    // بڕی ئەو خاڵەی لەگەڵ هەر زەبرێکدا لە یاریزان دەبڕدرێت
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|BloodTax", meta = (ClampMin = "0"))
    int32 PointsLostPerHit = 100;

    // ماوەی چالاکبوونی گەمەکە بە چرکە
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|BloodTax", meta = (ClampMin = "1.0"))
    float Duration = 60.0f;

private:
    FTimerHandle TimerHandle_Duration;
    TWeakObjectPtr<ACharacter> CachedOwner;
    TWeakObjectPtr<UGobbleGumComponent> CachedGumComponent;
    bool bEffectApplied = false;

    UFUNCTION()
    void OnDurationExpired();
};