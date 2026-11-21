#pragma once

#include "CoreMinimal.h"
#include "GobbleGumAction.h"
#include "GobbleGumAction_AegisBurst.generated.h"

class UHealthComponent;
class UGobbleGumComponent;

/**
 * Aegis Burst: کاتێک زەبرێکی کوشندەت بەردەکەوێت، لە بری کەوتنە سەر زەوی
 * خوێنت دەکاتەوە ١٠٠٪، بۆ ٣ چرکە دەتپارێزێت، و زۆمبییەکانی دەوروبەرت پاڵ دەنێت.
 */
UCLASS(Blueprintable, meta = (DisplayName = "GobbleGum Action: Aegis Burst"))
class LASTSTANDLEGACY_API UGobbleGumAction_AegisBurst : public UGobbleGumAction
{
    GENERATED_BODY()

public:
    UGobbleGumAction_AegisBurst();

    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner) override;
    virtual void DeactivateAction_Implementation(ACharacter* CharacterOwner) override;

protected:
    // بازنەی شەپۆلەکە لە دەوری یاریزان بە سانتیمەتر
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|AegisBurst", meta = (ClampMin = "50.0"))
    float BlastRadius = 450.0f;

    // ماوەی پارێزراوبوون دوای تەقینەوەکە بە چرکە
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|AegisBurst", meta = (ClampMin = "0.1"))
    float InvulnerabilityDuration = 3.0f;

    // هێزی پاڵنانی زۆمبییەکان بۆ دواوە
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|AegisBurst", meta = (ClampMin = "0.0"))
    float KnockbackStrength = 900.0f;

    // ماوەی سڕکردنی زۆمبییەکان (ئەگەر فەنکشنی Stun بەکاربهێنیت)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|AegisBurst", meta = (ClampMin = "0.1"))
    float StunDuration = 3.0f;

private:
    TWeakObjectPtr<ACharacter> CachedOwner;
    TWeakObjectPtr<UHealthComponent> CachedHealthComponent;
    TWeakObjectPtr<UGobbleGumComponent> CachedGumComponent;

    bool bEffectApplied = false;

    bool HandlePrePlayerDowned();
    void TriggerRadialBlast(ACharacter* OwnerChar);
};