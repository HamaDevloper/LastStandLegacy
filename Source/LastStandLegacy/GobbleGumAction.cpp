#include "GobbleGumAction.h"
#include "GameFramework/Character.h"
#include "GobbleGumComponent.h"

UGobbleGumAction::UGobbleGumAction()
{
    bIsActive = false;
}

void UGobbleGumAction::ActivateAction_Implementation(ACharacter* CharacterOwner)
{
    bIsActive = true;
}

void UGobbleGumAction::DeactivateAction_Implementation(ACharacter* CharacterOwner)
{
    bIsActive = false;
}

void UGobbleGumAction::NotifyActionFinished(ACharacter* CharacterOwner)
{
    DeactivateAction(CharacterOwner);

    if (CharacterOwner)
    {
        if (UGobbleGumComponent* GumComp = CharacterOwner->FindComponentByClass<UGobbleGumComponent>())
        {
            GumComp->Server_OnActionEffectEnded(this);
        }
    }
}