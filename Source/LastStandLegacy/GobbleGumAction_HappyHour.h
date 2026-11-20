#pragma once

#include "CoreMinimal.h"
#include "GobbleGumAction.h"
#include "GobbleGumAction_HappyHour.generated.h"

class ABasePerk;
class AHama;
class UGobbleGumComponent;

/**
 * گۆبڵگەمی Happy Hour: لە کاتی کڕینی هەر پێرکێک لە مەکینە، پێرکێکی تری هەڕەمەکی بە خۆڕایی دەبەخشێت
 */
UCLASS(Blueprintable, meta = (DisplayName = "GobbleGum Action: Happy Hour"))
class LASTSTANDLEGACY_API UGobbleGumAction_HappyHour : public UGobbleGumAction
{
    GENERATED_BODY()

public:
    UGobbleGumAction_HappyHour();

    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner) override;
    virtual void DeactivateAction_Implementation(ACharacter* CharacterOwner) override;

    bool TryGrantRandomBonusPerk(AHama* Player, FName ExcludedPerkID);

protected:
    // لیستی هەموو مەکینەکانی پێرک لەناو مەپەکەدا (هەمان شێوازی Random Perk)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|HappyHour")
    TArray<TSubclassOf<ABasePerk>> PerkMachineClasses;

private:
    TWeakObjectPtr<UGobbleGumComponent> CachedGumComponent;
};