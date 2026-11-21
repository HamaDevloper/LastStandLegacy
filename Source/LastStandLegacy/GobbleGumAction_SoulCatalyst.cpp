#include "GobbleGumAction_SoulCatalyst.h"
#include "Engine/World.h"
#include "Hama.h"
#include "HamaAbilityComponent.h"
#include "GobbleGumComponent.h"

UGobbleGumAction_SoulCatalyst::UGobbleGumAction_SoulCatalyst()
{
}

void UGobbleGumAction_SoulCatalyst::ActivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority()) return;
    if (bIsActive) return;

    AHama* HamaChar = Cast<AHama>(CharacterOwner);
    UHamaAbilityComponent* AbilityComp = HamaChar ? HamaChar->GetAbilityComponent() : nullptr;
    UGobbleGumComponent* GumComp = CharacterOwner->FindComponentByClass<UGobbleGumComponent>();
    if (!HamaChar || !AbilityComp || !GumComp) return;

    Super::ActivateAction_Implementation(CharacterOwner);

    CachedHama = HamaChar;
    CachedAbilityComponent = AbilityComp;
    CachedGumComponent = GumComp;

    ZombieKilledDelegateHandle = HamaChar->OnServerZombieKilled.AddUObject(this, &UGobbleGumAction_SoulCatalyst::HandleZombieKilled);
    bEffectApplied = true;

    if (UWorld* World = CharacterOwner->GetWorld())
    {
        World->GetTimerManager().ClearTimer(TimerHandle_Duration);
        World->GetTimerManager().SetTimer(
            TimerHandle_Duration,
            this,
            &UGobbleGumAction_SoulCatalyst::OnDurationExpired,
            Duration,
            false
        );
    }
}

void UGobbleGumAction_SoulCatalyst::HandleZombieKilled(AZombie* DeadZombie)
{
    if (!bIsActive || BonusPowerPerKill <= 0.0f) return;

    UHamaAbilityComponent* AbilityComp = CachedAbilityComponent.Get();
    if (!AbilityComp && CachedHama.IsValid())
    {
        AbilityComp = CachedHama->GetAbilityComponent();
        CachedAbilityComponent = AbilityComp;
    }

    if (AbilityComp)
    {
        AbilityComp->AddPower(BonusPowerPerKill);
    }
}

void UGobbleGumAction_SoulCatalyst::OnDurationExpired()
{
    AHama* OwnerChar = CachedHama.Get();
    UGobbleGumComponent* GumComp = CachedGumComponent.Get();

    DeactivateAction(OwnerChar);

    if (GumComp)
    {
        GumComp->Server_OnActionEffectEnded(this);
    }
}

void UGobbleGumAction_SoulCatalyst::DeactivateAction_Implementation(ACharacter* CharacterOwner)
{
    UWorld* World = CharacterOwner
        ? CharacterOwner->GetWorld()
        : (CachedHama.IsValid() ? CachedHama->GetWorld() : nullptr);

    if (World)
    {
        World->GetTimerManager().ClearTimer(TimerHandle_Duration);
    }

    if (bEffectApplied)
    {
        AHama* HamaChar = CachedHama.Get();
        if (!HamaChar && CharacterOwner)
        {
            HamaChar = Cast<AHama>(CharacterOwner);
        }

        if (HamaChar && ZombieKilledDelegateHandle.IsValid())
        {
            HamaChar->OnServerZombieKilled.Remove(ZombieKilledDelegateHandle);
        }
        ZombieKilledDelegateHandle.Reset();

        bEffectApplied = false;
    }

    CachedHama.Reset();
    CachedAbilityComponent.Reset();
    CachedGumComponent.Reset();

    Super::DeactivateAction_Implementation(CharacterOwner);
}