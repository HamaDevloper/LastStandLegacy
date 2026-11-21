#include "GobbleGumAction_MeleeBoost.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "GobbleGumComponent.h"

UGobbleGumAction_MeleeBoost::UGobbleGumAction_MeleeBoost()
{
}

void UGobbleGumAction_MeleeBoost::ActivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority() || bIsActive) return;

    UGobbleGumComponent* GumComp = CharacterOwner->FindComponentByClass<UGobbleGumComponent>();
    if (!GumComp) return;

    Super::ActivateAction_Implementation(CharacterOwner);

    CachedOwner = CharacterOwner;
    CachedGumComponent = GumComp;

    GumComp->ModifyMeleeInstaKillCount(1);
    bEffectApplied = true;

    if (UWorld* World = CharacterOwner->GetWorld())
    {
        World->GetTimerManager().ClearTimer(TimerHandle_Duration);
        World->GetTimerManager().SetTimer(
            TimerHandle_Duration,
            this,
            &UGobbleGumAction_MeleeBoost::OnDurationExpired,
            Duration,
            false
        );
    }
}

void UGobbleGumAction_MeleeBoost::OnDurationExpired()
{
    ACharacter* OwnerChar = CachedOwner.Get();
    UGobbleGumComponent* GumComp = CachedGumComponent.Get();

    DeactivateAction(OwnerChar);

    if (GumComp)
    {
        GumComp->Server_OnActionEffectEnded(this);
    }
}

void UGobbleGumAction_MeleeBoost::DeactivateAction_Implementation(ACharacter* CharacterOwner)
{
    UWorld* World = CharacterOwner
        ? CharacterOwner->GetWorld()
        : (CachedOwner.IsValid() ? CachedOwner->GetWorld() : nullptr);

    if (World)
    {
        World->GetTimerManager().ClearTimer(TimerHandle_Duration);
    }

    if (bEffectApplied)
    {
        UGobbleGumComponent* GumComp = CachedGumComponent.Get();
        if (!GumComp && CharacterOwner)
        {
            GumComp = CharacterOwner->FindComponentByClass<UGobbleGumComponent>();
        }

        if (GumComp)
        {
            GumComp->ModifyMeleeInstaKillCount(-1);
        }

        bEffectApplied = false;
    }

    CachedOwner.Reset();
    CachedGumComponent.Reset();

    Super::DeactivateAction_Implementation(CharacterOwner);
}