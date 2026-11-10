#include "GobbleGumAction_ShoppingFree.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "Hama.h"

UGobbleGumAction_ShoppingFree::UGobbleGumAction_ShoppingFree()
{
    Duration = 60.0f;
}

void UGobbleGumAction_ShoppingFree::ActivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority()) return;

    Super::ActivateAction_Implementation(CharacterOwner);

    CachedCharacterOwner = CharacterOwner;

    if (AHama* HamaChar = Cast<AHama>(CharacterOwner))
    {
        HamaChar->SetShoppingFreeActive(true);
    }

    UWorld* World = CharacterOwner->GetWorld();
    if (!World) return;

    World->GetTimerManager().ClearTimer(TimerHandle_Duration);
    World->GetTimerManager().SetTimer(
        TimerHandle_Duration,
        this,
        &UGobbleGumAction_ShoppingFree::OnDurationExpired,
        Duration,
        false
    );
}

void UGobbleGumAction_ShoppingFree::DeactivateAction_Implementation(ACharacter* CharacterOwner)
{
    ACharacter* TargetOwner = CharacterOwner ? CharacterOwner : CachedCharacterOwner.Get();

    if (TargetOwner && TargetOwner->HasAuthority())
    {
        if (AHama* HamaChar = Cast<AHama>(TargetOwner))
        {
            HamaChar->SetShoppingFreeActive(false);
        }

        if (UWorld* World = TargetOwner->GetWorld())
        {
            World->GetTimerManager().ClearTimer(TimerHandle_Duration);
        }
    }

    CachedCharacterOwner.Reset();
    Super::DeactivateAction_Implementation(CharacterOwner);
}

void UGobbleGumAction_ShoppingFree::OnDurationExpired()
{
    DeactivateAction(CachedCharacterOwner.Get());
}