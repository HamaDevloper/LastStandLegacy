#include "GobbleGumAction_VolatileCore.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "ThrowableComponent.h"
#include "GobbleGumComponent.h"
#include "Grenade.h"

UGobbleGumAction_VolatileCore::UGobbleGumAction_VolatileCore()
{
}

void UGobbleGumAction_VolatileCore::ActivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority() || bIsActive) return;

    UThrowableComponent* ThrowableComp = CharacterOwner->FindComponentByClass<UThrowableComponent>();
    UGobbleGumComponent* GumComp = CharacterOwner->FindComponentByClass<UGobbleGumComponent>();
    if (!ThrowableComp || !GumComp) return;

    Super::ActivateAction_Implementation(CharacterOwner);

    CachedOwner = CharacterOwner;
    CachedThrowableComponent = ThrowableComp;
    CachedGumComponent = GumComp;

    GrenadeThrownHandle = ThrowableComp->OnServerGrenadeThrown.AddUObject(
        this,
        &UGobbleGumAction_VolatileCore::HandleGrenadeThrown
    );
    bEffectApplied = true;

    if (UWorld* World = CharacterOwner->GetWorld())
    {
        World->GetTimerManager().ClearTimer(TimerHandle_Duration);
        World->GetTimerManager().SetTimer(
            TimerHandle_Duration,
            this,
            &UGobbleGumAction_VolatileCore::OnDurationExpired,
            Duration,
            false
        );
    }
}

void UGobbleGumAction_VolatileCore::HandleGrenadeThrown(AGrenade* ThrownGrenade)
{
    if (!bIsActive || !IsValid(ThrownGrenade)) return;

    ThrownGrenade->SetVolatileCore(true);
}

void UGobbleGumAction_VolatileCore::OnDurationExpired()
{
    ACharacter* OwnerChar = CachedOwner.Get();
    UGobbleGumComponent* GumComp = CachedGumComponent.Get();

    DeactivateAction(OwnerChar);

    if (GumComp)
    {
        GumComp->Server_OnActionEffectEnded(this);
    }
}

void UGobbleGumAction_VolatileCore::DeactivateAction_Implementation(ACharacter* CharacterOwner)
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
        UThrowableComponent* ThrowableComp = CachedThrowableComponent.Get();
        if (!ThrowableComp && CharacterOwner)
        {
            ThrowableComp = CharacterOwner->FindComponentByClass<UThrowableComponent>();
        }

        if (ThrowableComp && GrenadeThrownHandle.IsValid())
        {
            ThrowableComp->OnServerGrenadeThrown.Remove(GrenadeThrownHandle);
        }

        GrenadeThrownHandle.Reset();
        bEffectApplied = false;
    }

    CachedOwner.Reset();
    CachedThrowableComponent.Reset();
    CachedGumComponent.Reset();

    Super::DeactivateAction_Implementation(CharacterOwner);
}