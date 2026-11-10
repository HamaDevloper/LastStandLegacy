#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GobbleGumAction.generated.h"

class ACharacter;

/**
 * کلاسی بنەڕەتی بۆ تەواوی لۆژیکی کاریگەرییەکانی گۆبڵگەم
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class LASTSTANDLEGACY_API UGobbleGumAction : public UObject
{
    GENERATED_BODY()

public:
    UGobbleGumAction();

    // جێبەجێکردنی لۆژیکی گەمەکە - تەنها لەسەر سێرڤەر کاردەکات
    UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly, Category = "GobbleGum|Logic")
    void ActivateAction(ACharacter* CharacterOwner);
    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner);

    // کۆتاییهێنان بە کاریگەرییەکە (بۆ ئەو گەمانەی کاتین یان خولاوخولن)
    UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly, Category = "GobbleGum|Logic")
    void DeactivateAction(ACharacter* CharacterOwner);
    virtual void DeactivateAction_Implementation(ACharacter* CharacterOwner);

    // پشکنینی ئەوەی کە ئایا گەمەکە لە کاتی کارکردندایە یان نا
    UFUNCTION(BlueprintPure, Category = "GobbleGum|Logic")
    bool IsActionActive() const { return bIsActive; }

protected:
    UPROPERTY(BlueprintReadOnly, Category = "GobbleGum|State")
    bool bIsActive = false;
};