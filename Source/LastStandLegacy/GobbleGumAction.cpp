#include "GobbleGumAction.h"
#include "GameFramework/Character.h"

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