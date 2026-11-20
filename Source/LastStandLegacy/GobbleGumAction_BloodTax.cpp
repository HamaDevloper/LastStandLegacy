#include "GobbleGumAction_BloodTax.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "GobbleGumComponent.h"

UGobbleGumAction_BloodTax::UGobbleGumAction_BloodTax()
{
}

void UGobbleGumAction_BloodTax::ActivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority()) return;
    if (bIsActive) return;

    UGobbleGumComponent* GumComp = CharacterOwner->FindComponentByClass<UGobbleGumComponent>();
    if (!GumComp) return;

    Super::ActivateAction_Implementation(CharacterOwner);

    CachedOwner = CharacterOwner;
    CachedGumComponent = GumComp;

    GumComp->ModifyPointsMultiplier(ScoreMultiplier);
    GumComp->SetBloodTaxState(this, PointsLostPerHit);
    bEffectApplied = true;

    if (UWorld* World = CharacterOwner->GetWorld())
    {
        World->GetTimerManager().ClearTimer(TimerHandle_Duration);
        World->GetTimerManager().SetTimer(
            TimerHandle_Duration,
            this,
            &UGobbleGumAction_BloodTax::OnDurationExpired,
            Duration,
            false
        );
    }
}

void UGobbleGumAction_BloodTax::OnDurationExpired()
{
    ACharacter* OwnerChar = CachedOwner.Get();
    UGobbleGumComponent* GumComp = CachedGumComponent.Get();

    DeactivateAction(OwnerChar);

    if (GumComp)
    {
        GumComp->Server_OnActionEffectEnded(this);
    }
}

void UGobbleGumAction_BloodTax::ForceExpireFromBankruptcy()
{
    OnDurationExpired();
}

void UGobbleGumAction_BloodTax::DeactivateAction_Implementation(ACharacter* CharacterOwner)
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
            GumComp->ModifyPointsMultiplier(1.0f / FMath::Max(0.01f, ScoreMultiplier));
            GumComp->ClearBloodTaxState(this);
        }

        bEffectApplied = false;
    }

    CachedOwner.Reset();
    CachedGumComponent.Reset();

    Super::DeactivateAction_Implementation(CharacterOwner);
}