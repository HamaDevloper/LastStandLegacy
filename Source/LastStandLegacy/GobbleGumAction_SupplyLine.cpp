#include "GobbleGumAction_SupplyLine.h"
#include "Hama.h"
#include "BaseWeapon.h"
#include "GobbleGumComponent.h"

UGobbleGumAction_SupplyLine::UGobbleGumAction_SupplyLine()
{
}

void UGobbleGumAction_SupplyLine::ActivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority()) return;
    Super::ActivateAction_Implementation(CharacterOwner);

    UWorld* World = GetWorld();
    AHama* HamaChar = Cast<AHama>(CharacterOwner);
    if (!World || !HamaChar) return;

    // پاککردنەوەی تایمەرە کۆنەکان بۆ ڕێگریکردن لە Overlap
    World->GetTimerManager().ClearTimer(AmmoRegenTimerHandle);
    World->GetTimerManager().ClearTimer(DurationTimerHandle);

    // تایمەری پڕکردنەوەی فیشەک (هر ١٠ چرکە جارێک)
    World->GetTimerManager().SetTimer(
        AmmoRegenTimerHandle,
        FTimerDelegate::CreateWeakLambda(HamaChar, [HamaChar]()
            {
                if (IsValid(HamaChar))
                {
                    if (ABaseWeapon* CurrWep = HamaChar->GetCurrentWeapon())
                    {
                        if (IsValid(CurrWep))
                        {
                            CurrWep->AddReserveAmmo(CurrWep->GetMaxClipAmmo());
                        }
                    }
                }
            }),
        10.0f,
        true
    );

    const float GumDuration = 120.0f;

    // (DEBUG LOG): دیاریکردنی دەستپێکی گەومەکە
    UE_LOG(LogTemp, Log, TEXT("[SupplyLine] GobbleGum Activated. Duration: %f seconds."), GumDuration);

    World->GetTimerManager().SetTimer(
        DurationTimerHandle,
        FTimerDelegate::CreateWeakLambda(this, [this, CharacterOwner]()
            {
                // پشکنینی دیبەگ پێش تەواوبوون بۆ دڵنیابوون لەوەی یاریزانەکە هێشتا بوونی هەیە
                if (!IsValid(CharacterOwner))
                {
                    UE_LOG(LogTemp, Error, TEXT("[SupplyLine CRASH PREVENTED] CharacterOwner is NULL or PendingKill at duration end!"));
                    return;
                }

                UE_LOG(LogTemp, Log, TEXT("[SupplyLine] Duration reached zero. Calling NotifyActionFinished."));
                NotifyActionFinished(CharacterOwner);
            }),
        GumDuration,
        false
    );
}

void UGobbleGumAction_SupplyLine::DeactivateAction_Implementation(ACharacter* CharacterOwner)
{
    // (DEBUG LOG): دیاریکردنی چوونە ژوورەوە بۆ Deactivate
    UE_LOG(LogTemp, Warning, TEXT("[SupplyLine] DeactivateAction_Implementation CALLED."));
    Super::DeactivateAction_Implementation(CharacterOwner);

    if (!CharacterOwner)
    {
        UE_LOG(LogTemp, Error, TEXT("[SupplyLine CRASH] CharacterOwner is nullptr inside DeactivateAction!"));
        return;
    }

    if (CharacterOwner->HasAuthority())
    {
        if (UWorld* World = CharacterOwner->GetWorld())
        {
            World->GetTimerManager().ClearTimer(AmmoRegenTimerHandle);
            World->GetTimerManager().ClearTimer(DurationTimerHandle);
            UE_LOG(LogTemp, Log, TEXT("[SupplyLine] Timers cleared successfully in Deactivate."));
        }
    }

    if (UGobbleGumComponent* GumComp = CharacterOwner->FindComponentByClass<UGobbleGumComponent>())
    {
        if (IsValid(GumComp))
        {
            GumComp->ConsumeEventTriggeredGum();
        }
    }

#if WITH_EDITOR
    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Gum DeActivated Safely"));
#endif
}