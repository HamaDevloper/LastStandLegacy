#include "GobbleGumAction_LegacyKeeper.h"
#include "GameFramework/Character.h"
#include "HamaPlayerState.h"

UGobbleGumAction_LegacyKeeper::UGobbleGumAction_LegacyKeeper()
{
}

void UGobbleGumAction_LegacyKeeper::ActivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority() || bIsActive) return;

    AHamaPlayerState* PS = CharacterOwner->GetPlayerState<AHamaPlayerState>();
    if (!PS) return;

    Super::ActivateAction_Implementation(CharacterOwner);

    CachedPlayerState = PS;
    PS->SetLegacyKeeperArmed(true);
    bArmedByThisAction = true;
}

void UGobbleGumAction_LegacyKeeper::DeactivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (bArmedByThisAction)
    {
        AHamaPlayerState* PS = CachedPlayerState.Get();
        if (!PS && CharacterOwner)
        {
            PS = CharacterOwner->GetPlayerState<AHamaPlayerState>();
        }

        if (PS)
        {
            PS->SetLegacyKeeperArmed(false);
        }

        bArmedByThisAction = false;
    }

    CachedPlayerState.Reset();

    Super::DeactivateAction_Implementation(CharacterOwner);
}