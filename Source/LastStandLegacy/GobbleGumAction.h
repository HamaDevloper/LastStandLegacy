#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GobbleGumAction.generated.h"

class ACharacter;
class UGobbleGumComponent;

UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class LASTSTANDLEGACY_API UGobbleGumAction : public UObject
{
    GENERATED_BODY()

public:
    UGobbleGumAction();

    UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly, Category = "GobbleGum|Logic")
    void ActivateAction(ACharacter* CharacterOwner);
    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner);

    UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly, Category = "GobbleGum|Logic")
    void DeactivateAction(ACharacter* CharacterOwner);
    virtual void DeactivateAction_Implementation(ACharacter* CharacterOwner);

    UFUNCTION(BlueprintPure, Category = "GobbleGum|Logic")
    bool IsActionActive() const { return bIsActive; }

protected:
    // بانگ دەکرێت کاتێک کاریگەریی گەمێکی کاتی یان دەستبەجێ بەتەواوی کۆتایی دێت
    void NotifyActionFinished(ACharacter* CharacterOwner);

    UPROPERTY(BlueprintReadOnly, Category = "GobbleGum|State")
    bool bIsActive = false;
};