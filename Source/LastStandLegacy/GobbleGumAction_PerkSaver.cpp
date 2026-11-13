#include "GobbleGumAction_PerkSaver.h"
#include "GameFramework/Character.h"
#include "GobbleGumComponent.h"

UGobbleGumAction_PerkSaver::UGobbleGumAction_PerkSaver()
{
}

void UGobbleGumAction_PerkSaver::ActivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority()) return;

    if (bIsActive) return;

    UGobbleGumComponent* GumComp = CharacterOwner->FindComponentByClass<UGobbleGumComponent>();
    if (!GumComp) return;

    Super::ActivateAction_Implementation(CharacterOwner);

    CachedGumComponent = GumComp;
    GumComp->ModifyPreservePerksCount(1);
}

void UGobbleGumAction_PerkSaver::DeactivateAction_Implementation(ACharacter* CharacterOwner)
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
            GumComp->ModifyPreservePerksCount(-1);
        }
    }

    CachedGumComponent.Reset();
    Super::DeactivateAction_Implementation(CharacterOwner);
}