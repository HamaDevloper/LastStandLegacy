#include "GobbleGumAction_CopperMagnet.h"
#include "GameFramework/Character.h"
#include "GobbleGumComponent.h"

UGobbleGumAction_CopperMagnet::UGobbleGumAction_CopperMagnet()
{
}

void UGobbleGumAction_CopperMagnet::ActivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority() || bIsActive) return;

    UGobbleGumComponent* GumComp = CharacterOwner->FindComponentByClass<UGobbleGumComponent>();
    if (!GumComp) return;

    Super::ActivateAction_Implementation(CharacterOwner);

    CachedGumComponent = GumComp;
    GumComp->SetPointsMultiplier(Multiplier);
    bEffectApplied = true;
}

void UGobbleGumAction_CopperMagnet::DeactivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (bEffectApplied)
    {
        UGobbleGumComponent* GumComp = CachedGumComponent.Get();
        if (!GumComp && CharacterOwner)
        {
            GumComp = CharacterOwner->FindComponentByClass<UGobbleGumComponent>();
        }

        if (GumComp)
        {
            GumComp->SetPointsMultiplier(1.0f);
        }

        bEffectApplied = false;
    }

    CachedGumComponent.Reset();

    Super::DeactivateAction_Implementation(CharacterOwner);
}