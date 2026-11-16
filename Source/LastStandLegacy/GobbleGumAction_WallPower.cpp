#include "GobbleGumAction_WallPower.h"
#include "GameFramework/Character.h"
#include "GobbleGumComponent.h"

UGobbleGumAction_WallPower::UGobbleGumAction_WallPower()
{
}

void UGobbleGumAction_WallPower::ActivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority()) return;
    if (bIsActive) return;

    UGobbleGumComponent* GumComp = CharacterOwner->FindComponentByClass<UGobbleGumComponent>();
    if (!GumComp) return;

    Super::ActivateAction_Implementation(CharacterOwner);

    CachedGumComponent = GumComp;
    GumComp->ModifyAutoUpgradeWallBuyCount(1);
}

void UGobbleGumAction_WallPower::DeactivateAction_Implementation(ACharacter* CharacterOwner)
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
            GumComp->ModifyAutoUpgradeWallBuyCount(-1);
        }
    }

    CachedGumComponent.Reset();
    Super::DeactivateAction_Implementation(CharacterOwner);
}