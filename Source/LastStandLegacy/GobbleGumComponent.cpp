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
}

void UGobbleGumComponent::TryActivateCurrentGum()
{
    if (!CurrentActiveGum || RemainingUses == 0) return;
    if (CurrentActiveGum->ActivationType != EGobbleGumActivationType::Activated) return;

    const float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastActivationTime < ActivationCooldown)
    {
        return;
    }

    Server_ActivateCurrentGum();
}

void UGobbleGumComponent::Server_ActivateCurrentGum_Implementation()
{
    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
    if (!OwnerChar || !CurrentActiveGum || RemainingUses == 0) return;
    if (CurrentActiveGum->ActivationType != EGobbleGumActivationType::Activated) return;

    const float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastActivationTime < ActivationCooldown)
    {
        return;
    }
    LastActivationTime = CurrentTime;

    if (CurrentActiveGum->ActionLogic)
    {
        CurrentActiveGum->ActionLogic->ActivateAction(OwnerChar);
    }

    RemainingUses--;
    MARK_PROPERTY_DIRTY_FROM_NAME(UGobbleGumComponent, RemainingUses, this);

    if (RemainingUses == 0)
    {
        Server_ClearCurrentGum();
    }
    else
    {
        if (OwnerChar->IsLocallyControlled())
        {
            OnRep_RemainingUses();
        }
    }
}

void UGobbleGumComponent::Server_SetCurrentGum(UGobbleGumData* NewGum)
{
    if (!GetOwner()->HasAuthority() || !NewGum) return;

    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());

    if (CurrentActiveGum && CurrentActiveGum->ActionLogic)
    {
        CurrentActiveGum->ActionLogic->DeactivateAction(OwnerChar);
    }

    CurrentActiveGum = NewGum;
    RemainingUses = NewGum->MaxUses;

    MARK_PROPERTY_DIRTY_FROM_NAME(UGobbleGumComponent, CurrentActiveGum, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(UGobbleGumComponent, RemainingUses, this);

    if (CurrentActiveGum->ActivationType == EGobbleGumActivationType::Immediate)
    {
        if (CurrentActiveGum->ActionLogic && OwnerChar)
        {
            CurrentActiveGum->ActionLogic->ActivateAction(OwnerChar);
        }

        CurrentActiveGum = nullptr;
        RemainingUses = 0;

        MARK_PROPERTY_DIRTY_FROM_NAME(UGobbleGumComponent, CurrentActiveGum, this);
        MARK_PROPERTY_DIRTY_FROM_NAME(UGobbleGumComponent, RemainingUses, this);
    }

    if (OwnerChar && OwnerChar->IsLocallyControlled())
    {
        OnRep_CurrentActiveGum();
        OnRep_RemainingUses();
    }
}

void UGobbleGumComponent::Server_ClearCurrentGum()
{
    if (!GetOwner()->HasAuthority()) return;

    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());

    if (CurrentActiveGum && CurrentActiveGum->ActionLogic)
    {
        CurrentActiveGum->ActionLogic->DeactivateAction(OwnerChar);
    }

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

    // ئەگەر دەستەکە بەتاڵ بووبێت، سەرلەنوێ هەموو ٥ دانەکە تێکەڵ دەکەینەوە
    if (RemainingDeck.Num() == 0)
    {
        RemainingDeck = EquippedGums;
    }

    // هەڵبژاردنی یەک دانە بە ڕاندۆم لەو گەمانەی ماونەتەوە
    const int32 PickIndex = FMath::RandRange(0, RemainingDeck.Num() - 1);
    UGobbleGumData* SelectedGum = RemainingDeck[PickIndex];

    // لادانی ئەو گەمە تا خولەکە تەواو نەبێت دووبارە دەرنەچێتەوە
    RemainingDeck.RemoveAt(PickIndex);

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