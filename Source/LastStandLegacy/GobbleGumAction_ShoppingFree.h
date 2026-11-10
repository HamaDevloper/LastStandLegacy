#pragma once

#include "CoreMinimal.h"
#include "GobbleGumAction.h"
#include "GobbleGumAction_ShoppingFree.generated.h"

UCLASS()
class LASTSTANDLEGACY_API UGobbleGumAction_ShoppingFree : public UGobbleGumAction
{
    GENERATED_BODY()

public:
    UGobbleGumAction_ShoppingFree();

    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner) override;
    virtual void DeactivateAction_Implementation(ACharacter* CharacterOwner) override;

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ShoppingFree", meta = (ClampMin = "5.0"))
    float Duration = 60.0f;

    FTimerHandle TimerHandle_Duration;

    UPROPERTY()
    TWeakObjectPtr<ACharacter> CachedCharacterOwner;

private:
    void OnDurationExpired();
};