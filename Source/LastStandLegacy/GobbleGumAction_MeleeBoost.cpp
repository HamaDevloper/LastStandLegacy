#include "GobbleGumAction_MeleeBoost.h"
#include "GameFramework/Character.h"
#include "GobbleGumComponent.h"

UGobbleGumAction_MeleeBoost::UGobbleGumAction_MeleeBoost()
{
}

void UGobbleGumAction_MeleeBoost::ActivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority()) return;
    if (bIsActive) return;

    UGobbleGumComponent* GumComp = CharacterOwner->FindComponentByClass<UGobbleGumComponent>();
    if (!GumComp) return;

    Super::ActivateAction_Implementation(CharacterOwner);

    CachedGumComponent = GumComp;
    GumComp->ModifyMeleeInstaKillCount(1);
}

void UGobbleGumAction_MeleeBoost::DeactivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (bIsActive)
    {
        UGobbleGumComponent* GumComp = CachedGumComponent.Get();
        if (!GumComp && CharacterOwner)
        {
            GumComp = CharacterOwner->FindComponentByClass<UGobbleGumComponent>();
        }

        if (GumComp)
        {
            GumComp->ModifyMeleeInstaKillCount(-1);
        }
    }

    CachedGumComponent.Reset();
    Super::DeactivateAction_Implementation(CharacterOwner);
}