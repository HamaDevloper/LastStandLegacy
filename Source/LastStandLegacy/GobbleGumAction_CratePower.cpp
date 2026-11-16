#include "GobbleGumAction_CratePower.h"
#include "GameFramework/Character.h"
#include "GobbleGumComponent.h"

UGobbleGumAction_CratePower::UGobbleGumAction_CratePower()
{
}

void UGobbleGumAction_CratePower::ActivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority()) return;
    if (bIsActive) return;

    UGobbleGumComponent* GumComp = CharacterOwner->FindComponentByClass<UGobbleGumComponent>();
    if (!GumComp) return;

    Super::ActivateAction_Implementation(CharacterOwner);

    CachedGumComponent = GumComp;
    GumComp->ModifyAutoUpgradeBoxWeaponCount(1);
}

void UGobbleGumAction_CratePower::DeactivateAction_Implementation(ACharacter* CharacterOwner)
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
            GumComp->ModifyAutoUpgradeBoxWeaponCount(-1);
        }
    }

    CachedGumComponent.Reset();
    Super::DeactivateAction_Implementation(CharacterOwner);
}