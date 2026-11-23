#include "GobbleGumAction_GraveHold.h"
#include "GameFramework/Character.h"
#include "HealthComponent.h"

UGobbleGumAction_GraveHold::UGobbleGumAction_GraveHold()
{
}

void UGobbleGumAction_GraveHold::ActivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority() || bIsActive) return;

    UHealthComponent* HealthComp = CharacterOwner->FindComponentByClass<UHealthComponent>();
    if (!HealthComp) return;

    Super::ActivateAction_Implementation(CharacterOwner);

    CachedHealthComponent = HealthComp;
    HealthComp->SetBleedoutMultiplier(BleedoutTimeMultiplier);
    bEffectApplied = true;
}

void UGobbleGumAction_GraveHold::DeactivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (bEffectApplied)
    {
        UHealthComponent* HealthComp = CachedHealthComponent.Get();
        if (!HealthComp && CharacterOwner)
        {
            HealthComp = CharacterOwner->FindComponentByClass<UHealthComponent>();
        }

        if (HealthComp)
        {
            HealthComp->SetBleedoutMultiplier(1.0f);
        }

        bEffectApplied = false;
    }

    CachedHealthComponent.Reset();

    Super::DeactivateAction_Implementation(CharacterOwner);
}