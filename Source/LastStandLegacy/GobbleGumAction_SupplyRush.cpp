#include "GobbleGumAction_SupplyRush.h"
#include "GameFramework/Character.h"
#include "LastStandLegacyGameState.h"

UGobbleGumAction_SupplyRush::UGobbleGumAction_SupplyRush()
{
}

void UGobbleGumAction_SupplyRush::ActivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority()) return;

    if (bIsActive) return;

    UWorld* World = CharacterOwner->GetWorld();
    ALastStandLegacyGameState* GS = World ? World->GetGameState<ALastStandLegacyGameState>() : nullptr;
    if (!GS) return;

    Super::ActivateAction_Implementation(CharacterOwner);

    CachedWorld = World;
    GS->ActivePowerVacuumCount++;
}

void UGobbleGumAction_SupplyRush::DeactivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (bIsActive)
    {
        UWorld* World = CharacterOwner ? CharacterOwner->GetWorld() : CachedWorld.Get();
        if (ALastStandLegacyGameState* GS = World ? World->GetGameState<ALastStandLegacyGameState>() : nullptr)
        {
            GS->ActivePowerVacuumCount = FMath::Max(0, GS->ActivePowerVacuumCount - 1);
        }
    }

    CachedWorld.Reset();
    Super::DeactivateAction_Implementation(CharacterOwner);
}