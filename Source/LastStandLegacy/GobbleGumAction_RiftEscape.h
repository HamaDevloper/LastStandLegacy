#pragma once

#include "CoreMinimal.h"
#include "GobbleGumAction.h"
#include "GobbleGumAction_RiftEscape.generated.h"

class ACharacter;

UCLASS(Blueprintable, EditInlineNew)
class LASTSTANDLEGACY_API UGobbleGumAction_RiftEscape : public UGobbleGumAction
{
    GENERATED_BODY()

public:
    UGobbleGumAction_RiftEscape();

    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner) override;

protected:
    // کەمترین مەودا (بە سانتیمەتر) بۆ ئەوەی یاریزان لە ناو هەمان ژووردا نەخاتەوە سەر خاڵێکی زۆر نزیک (1500 = 15 مەتر)
    UPROPERTY(EditDefaultsOnly, Category = "RiftEscape")
    float MinTeleportDistance = 1500.0f;

    // ئەگەر بکرێتە True، تەنانەت ژوورە داخراوەکانیش دەکرێنەوە بۆ تەلەپۆرت (بۆ Speedrun)
    UPROPERTY(EditDefaultsOnly, Category = "RiftEscape")
    bool bAllowLockedZones = false;
};