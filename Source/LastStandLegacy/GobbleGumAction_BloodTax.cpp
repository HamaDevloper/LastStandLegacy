#include "GobbleGumAction_BloodTax.h"
#include "Engine/World.h"
#include "Hama.h"
#include "HamaPlayerState.h"
#include "GobbleGumComponent.h"

UGobbleGumAction_BloodTax::UGobbleGumAction_BloodTax()
{
}

void UGobbleGumAction_BloodTax::ActivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority()) return;
    if (bIsActive) return;

    AHama* HamaChar = Cast<AHama>(CharacterOwner);
    UGobbleGumComponent* GumComp = CharacterOwner->FindComponentByClass<UGobbleGumComponent>();
    if (!HamaChar || !GumComp) return;

    Super::ActivateAction_Implementation(CharacterOwner);

    CachedHama = HamaChar;
    CachedPlayerState = HamaChar->GetPlayerState<AHamaPlayerState>();
    CachedGumComponent = GumComp;

    GumComp->ModifyPointsMultiplier(ScoreMultiplier);
    DamageTakenDelegateHandle = HamaChar->OnServerDamageTaken.AddUObject(this, &UGobbleGumAction_BloodTax::HandleOwnerDamaged);
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

void UGobbleGumAction_BloodTax::HandleOwnerDamaged(float ActualDamage, AActor* DamageCauser)
{
    if (!bIsActive || PointsLostPerHit <= 0) return;

    AHamaPlayerState* PS = CachedPlayerState.Get();
    if (!PS && CachedHama.IsValid())
    {
        PS = CachedHama->GetPlayerState<AHamaPlayerState>();
        CachedPlayerState = PS;
    }

    if (PS)
    {
        PS->RemovePoints(PointsLostPerHit);

        if (PS->GetPoints() <= 0)
        {
            OnDurationExpired();
        }
    }
}

void UGobbleGumAction_BloodTax::OnDurationExpired()
{
    AHama* OwnerChar = CachedHama.Get();
    UGobbleGumComponent* GumComp = CachedGumComponent.Get();

    DeactivateAction(OwnerChar);

    if (GumComp)
    {
        GumComp->Server_OnActionEffectEnded(this);
    }
}

void UGobbleGumAction_BloodTax::DeactivateAction_Implementation(ACharacter* CharacterOwner)
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

        if (HamaChar && DamageTakenDelegateHandle.IsValid())
        {
            HamaChar->OnServerDamageTaken.Remove(DamageTakenDelegateHandle);
        }
        DamageTakenDelegateHandle.Reset();

        UGobbleGumComponent* GumComp = CachedGumComponent.Get();
        if (!GumComp && CharacterOwner)
        {
            GumComp = CharacterOwner->FindComponentByClass<UGobbleGumComponent>();
        }

        if (GumComp)
        {
            GumComp->ModifyPointsMultiplier(1.0f / FMath::Max(0.01f, ScoreMultiplier));
        }

        bEffectApplied = false;
    }

    CachedHama.Reset();
    CachedPlayerState.Reset();
    CachedGumComponent.Reset();

    Super::DeactivateAction_Implementation(CharacterOwner);
}