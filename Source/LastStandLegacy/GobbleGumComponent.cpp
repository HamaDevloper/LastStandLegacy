#include "GobbleGumComponent.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "GameFramework/Character.h"
#include "GobbleGumData.h"
#include "GobbleGumAction.h"

UGobbleGumComponent::UGobbleGumComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UGobbleGumComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams PushParams;
    PushParams.bIsPushBased = true;

    DOREPLIFETIME_WITH_PARAMS_FAST(UGobbleGumComponent, CurrentActiveGum, PushParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(UGobbleGumComponent, RemainingUses, PushParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(UGobbleGumComponent, PurchasesThisRound, PushParams);
}

void UGobbleGumComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // پاککردنەوەی هەر تایمەرێک یان کاریگەرییەکی چالاک ئەگەر کارەکتەرەکە لەناوچوو
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        if (ActiveActionInstance && ActiveActionInstance->IsActionActive())
        {
            ActiveActionInstance->DeactivateAction(Cast<ACharacter>(GetOwner()));
        }
        ActiveActionInstance = nullptr;
    }

    Super::EndPlay(EndPlayReason);
}

void UGobbleGumComponent::IncrementPurchasesThisRound()
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        PurchasesThisRound++;
        MARK_PROPERTY_DIRTY_FROM_NAME(UGobbleGumComponent, PurchasesThisRound, this);
    }
}

void UGobbleGumComponent::Server_OnNewRoundStarted()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    // ١. سفرکردنەوەی ژمارەی کڕینەکان و گەڕاندنەوەی نرخ بۆ دۆخی ئاسایی لە خولی نوێدا
    if (PurchasesThisRound != 0)
    {
        PurchasesThisRound = 0;
        MARK_PROPERTY_DIRTY_FROM_NAME(UGobbleGumComponent, PurchasesThisRound, this);
    }

    // ٢. کەمکردنەوەی خول بۆ ئەو گەمانەی بەپێی Round کار دەکەن
    if (CurrentActiveGum && CurrentActiveGum->ActivationType == EGobbleGumActivationType::RoundBased)
    {
        if (RemainingUses > 0)
        {
            RemainingUses--;
            MARK_PROPERTY_DIRTY_FROM_NAME(UGobbleGumComponent, RemainingUses, this);

            if (ACharacter* OwnerChar = Cast<ACharacter>(GetOwner()))
            {
                if (OwnerChar->IsLocallyControlled())
                {
                    OnRep_RemainingUses();
                }
            }
        }

        if (RemainingUses == 0)
        {
            Server_ClearCurrentGum();
        }
    }
}

void UGobbleGumComponent::TryActivateCurrentGum()
{
    if (!CurrentActiveGum || RemainingUses == 0) return;
    if (CurrentActiveGum->ActivationType != EGobbleGumActivationType::Activated) return;

    UWorld* World = GetWorld();
    if (!World) return;

    const float CurrentTime = World->GetTimeSeconds();
    if (LastClientRequestTime >= 0.0f && (CurrentTime - LastClientRequestTime < ActivationCooldown))
    {
        return;
    }
    LastClientRequestTime = CurrentTime;

    Server_ActivateCurrentGum();
}

void UGobbleGumComponent::Server_ActivateCurrentGum_Implementation()
{
    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
    if (!OwnerChar || !CurrentActiveGum || RemainingUses == 0) return;
    if (CurrentActiveGum->ActivationType != EGobbleGumActivationType::Activated) return;

    // ئەگەر گەمەکە هێشتا لە دۆخی کارکردندا بێت، ڕێگە مەدە دووبارە چالاک بکرێتەوە
    if (ActiveActionInstance && ActiveActionInstance->IsActionActive()) return;

    UWorld* World = GetWorld();
    if (!World) return;

    const float CurrentTime = World->GetTimeSeconds();
    if (LastServerActivationTime >= 0.0f && (CurrentTime - LastServerActivationTime < ActivationCooldown))
    {
        return;
    }
    LastServerActivationTime = CurrentTime;

    RemainingUses--;
    MARK_PROPERTY_DIRTY_FROM_NAME(UGobbleGumComponent, RemainingUses, this);

    if (OwnerChar->IsLocallyControlled())
    {
        OnRep_RemainingUses();
    }

    if (ActiveActionInstance)
    {
        ActiveActionInstance->ActivateAction(OwnerChar);
    }

    // تەنها ئەگەر کاریگەرییەکە تایمەری لەسەر نەبوو (یەکسەر تەواو بوو) و هیچ بەکارهێنانێکی نەما، پاکی دەکەینەوە
    if (RemainingUses == 0 && (!ActiveActionInstance || !ActiveActionInstance->IsActionActive()))
    {
        Server_ClearCurrentGum();
    }
}

void UGobbleGumComponent::Server_SetCurrentGum(UGobbleGumData* NewGum)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !NewGum) return;

    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());

    // کوژاندنەوەی کاریگەریی گەمی پێشوو ئەگەر هێشتا چالاک بوو
    if (ActiveActionInstance && ActiveActionInstance->IsActionActive())
    {
        ActiveActionInstance->DeactivateAction(OwnerChar);
    }
    ActiveActionInstance = nullptr;

    CurrentActiveGum = NewGum;

    if (NewGum->ActivationType == EGobbleGumActivationType::RoundBased)
    {
        RemainingUses = FMath::Max<uint8>(1, NewGum->DurationRounds);
    }
    else if (NewGum->ActivationType == EGobbleGumActivationType::Immediate)
    {
        RemainingUses = 0;
    }
    else
    {
        RemainingUses = NewGum->MaxUses;
    }

    MARK_PROPERTY_DIRTY_FROM_NAME(UGobbleGumComponent, CurrentActiveGum, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(UGobbleGumComponent, RemainingUses, this);

    if (NewGum->ActionLogic)
    {
        ActiveActionInstance = DuplicateObject<UGobbleGumAction>(NewGum->ActionLogic, this);
    }

    // چالاککردنی دەستبەجێ بۆ جۆرەکانی Immediate و RoundBased
    if (NewGum->ActivationType == EGobbleGumActivationType::Immediate ||
        NewGum->ActivationType == EGobbleGumActivationType::RoundBased)
    {
        if (ActiveActionInstance && OwnerChar)
        {
            ActiveActionInstance->ActivateAction(OwnerChar);
        }

        // ئەگەر گەمەکە Immediate بوو و کاریگەرییەکەی یەکسەر کۆتایی هات (وەک Perkaholic)، لەسەر شاشە لایدەبەین
        if (NewGum->ActivationType == EGobbleGumActivationType::Immediate &&
            (!ActiveActionInstance || !ActiveActionInstance->IsActionActive()))
        {
            CurrentActiveGum = nullptr;
            ActiveActionInstance = nullptr;
            MARK_PROPERTY_DIRTY_FROM_NAME(UGobbleGumComponent, CurrentActiveGum, this);
        }
    }

    if (OwnerChar && OwnerChar->IsLocallyControlled())
    {
        OnRep_CurrentActiveGum();
        OnRep_RemainingUses();
    }
}

void UGobbleGumComponent::Server_OnActionEffectEnded(UGobbleGumAction* FinishedAction)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    if (FinishedAction == ActiveActionInstance && RemainingUses == 0)
    {
        Server_ClearCurrentGum();
    }
}

void UGobbleGumComponent::Server_ClearCurrentGum()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());

    if (ActiveActionInstance && ActiveActionInstance->IsActionActive())
    {
        ActiveActionInstance->DeactivateAction(OwnerChar);
    }

    ActiveActionInstance = nullptr;
    CurrentActiveGum = nullptr;
    RemainingUses = 0;

    MARK_PROPERTY_DIRTY_FROM_NAME(UGobbleGumComponent, CurrentActiveGum, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(UGobbleGumComponent, RemainingUses, this);

    if (OwnerChar && OwnerChar->IsLocallyControlled())
    {
        OnRep_CurrentActiveGum();
        OnRep_RemainingUses();
    }
}

UGobbleGumData* UGobbleGumComponent::RollNextGumFromPack()
{
    if (EquippedGums.Num() == 0) return nullptr;

    if (RemainingDeck.Num() == 0)
    {
        for (UGobbleGumData* Gum : EquippedGums)
        {
            if (IsValid(Gum))
            {
                RemainingDeck.Add(Gum);
            }
        }
    }

    if (RemainingDeck.Num() == 0) return nullptr;

    const int32 PickIndex = FMath::RandRange(0, RemainingDeck.Num() - 1);
    UGobbleGumData* SelectedGum = RemainingDeck[PickIndex];

    RemainingDeck.RemoveAtSwap(PickIndex);

    return SelectedGum;
}

void UGobbleGumComponent::OnRep_CurrentActiveGum()
{
    OnCurrentGumChanged.Broadcast(CurrentActiveGum);
}

void UGobbleGumComponent::OnRep_RemainingUses()
{
    OnUsesRemainingChanged.Broadcast(RemainingUses);
}