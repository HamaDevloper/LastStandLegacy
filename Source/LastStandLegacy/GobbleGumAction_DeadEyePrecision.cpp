#include "GobbleGumAction_DeadEyePrecision.h"
#include "Hama.h"
#include "BaseWeapon.h"
#include "GobbleGumComponent.h"

UGobbleGumAction_DeadEyePrecision::UGobbleGumAction_DeadEyePrecision()
{
}

void UGobbleGumAction_DeadEyePrecision::ActivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority()) return;
    Super::ActivateAction_Implementation(CharacterOwner);

    UWorld* World = GetWorld();
    UGobbleGumComponent* GumComp = CharacterOwner->FindComponentByClass<UGobbleGumComponent>();

    if (!World || !GumComp) return;

    CachedGumComp = GumComp;

    GumComp->ModifyDeadEyeCount(1);

    World->GetTimerManager().ClearTimer(DurationTimerHandle);

    const float GumDuration = 60.0f;
    World->GetTimerManager().SetTimer(
        DurationTimerHandle,
        FTimerDelegate::CreateWeakLambda(this, [this, CharacterOwner]()
            {
                if (IsValid(CharacterOwner))
                {
                    NotifyActionFinished(CharacterOwner);
                }
            }),
        GumDuration,
        false
    );
}

void UGobbleGumAction_DeadEyePrecision::DeactivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (!bIsActive || !CharacterOwner) return;

    Super::DeactivateAction_Implementation(CharacterOwner);

    if (CharacterOwner->HasAuthority())
    {
        if (UWorld* World = CharacterOwner->GetWorld())
        {
            World->GetTimerManager().ClearTimer(DurationTimerHandle);
        }

        UGobbleGumComponent* GumComp = CachedGumComp.Get();
        if (!GumComp)
        {
            GumComp = CharacterOwner->FindComponentByClass<UGobbleGumComponent>();
        }

        if (IsValid(GumComp))
        {
            GumComp->ModifyDeadEyeCount(-1);
            GumComp->ConsumeEventTriggeredGum();
        }
    }
}