#include "GobbleGumAction_Overclocked.h"
#include "GameFramework/Character.h"
#include "GobbleGumComponent.h"

UGobbleGumAction_Overclocked::UGobbleGumAction_Overclocked()
{
}

void UGobbleGumAction_Overclocked::ActivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority() || bIsActive) return;

    UGobbleGumComponent* GumComp = CharacterOwner->FindComponentByClass<UGobbleGumComponent>();
    if (!GumComp) return;

    Super::ActivateAction_Implementation(CharacterOwner);

    CachedGumComponent = GumComp;
    GumComp->SetOverclockedCircuit(true);
}

void UGobbleGumAction_Overclocked::DeactivateAction_Implementation(ACharacter* CharacterOwner)
{
    UGobbleGumComponent* GumComp = CachedGumComponent.Get();
    if (!GumComp && CharacterOwner)
    {
        GumComp = CharacterOwner->FindComponentByClass<UGobbleGumComponent>();
    }

    if (GumComp)
    {
        GumComp->SetOverclockedCircuit(false);
    }

    CachedGumComponent.Reset();

    Super::DeactivateAction_Implementation(CharacterOwner);
}