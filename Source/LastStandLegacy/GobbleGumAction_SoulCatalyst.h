#pragma once

#include "CoreMinimal.h"
#include "GobbleGumAction.h"
#include "TimerManager.h"
#include "GobbleGumAction_SoulCatalyst.generated.h"

class AHama;
class AZombie;
class UHamaAbilityComponent;
class UGobbleGumComponent;

/**
 * Soul Catalyst: بۆ ماوەی ٤٥ چرکە، کوشتنی هەر زۆمبییەک پاوەرێکی زۆر زیاتر دەدات
 * بە ئەبیلیتییەکەت (تەنها کاتێک ئەبیلیتییەکەت چالاک نییە).
 */
UCLASS(Blueprintable, meta = (DisplayName = "GobbleGum Action: Soul Catalyst"))
class LASTSTANDLEGACY_API UGobbleGumAction_SoulCatalyst : public UGobbleGumAction
{
    GENERATED_BODY()

public:
    UGobbleGumAction_SoulCatalyst();

    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner) override;
    virtual void DeactivateAction_Implementation(ACharacter* CharacterOwner) override;

protected:
    // بڕی پاوەری زیادە بۆ هەر کوشتنێک (کوشتنی ئاسایی ٢ تا ٤ دەدات)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|SoulCatalyst", meta = (ClampMin = "0.1"))
    float BonusPowerPerKill = 12.0f;

    // ماوەی چالاکبوونی گەمەکە بە چرکە
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|SoulCatalyst", meta = (ClampMin = "1.0"))
    float Duration = 45.0f;

private:
    FTimerHandle TimerHandle_Duration;
    FDelegateHandle ZombieKilledDelegateHandle;

    TWeakObjectPtr<AHama> CachedHama;
    TWeakObjectPtr<UHamaAbilityComponent> CachedAbilityComponent;
    TWeakObjectPtr<UGobbleGumComponent> CachedGumComponent;

    bool bEffectApplied = false;

    void HandleZombieKilled(AZombie* DeadZombie);

    UFUNCTION()
    void OnDurationExpired();
};