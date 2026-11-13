#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/Core/PushModel/PushModel.h"
#include "GobbleGumTypes.h"
#include "GobbleGumComponent.generated.h"

class UGobbleGumData;
class ACharacter;
class UGobbleGumAction;
class UAnimMontage;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCurrentGumChanged, const UGobbleGumData*, NewGum);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUsesRemainingChanged, uint8, UsesLeft);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LASTSTANDLEGACY_API UGobbleGumComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGobbleGumComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    void Server_OnNewRoundStarted();
    void Server_OnActionEffectEnded(UGobbleGumAction* FinishedAction);

    UFUNCTION(BlueprintCallable, Category = "GobbleGum")
    void TryActivateCurrentGum();

    UFUNCTION(BlueprintAuthorityOnly, Category = "GobbleGum")
    void Server_SetCurrentGum(UGobbleGumData* NewGum);

    UFUNCTION(BlueprintAuthorityOnly, Category = "GobbleGum")
    void Server_ClearCurrentGum();

    UFUNCTION(BlueprintPure, Category = "GobbleGum")
    UGobbleGumData* GetCurrentGum() const { return CurrentActiveGum; }

    UFUNCTION(BlueprintPure, Category = "GobbleGum")
    uint8 GetRemainingUses() const { return RemainingUses; }

    UGobbleGumData* RollNextGumFromPack();

    void IncrementPurchasesThisRound();
    uint8 GetPurchasesThisRound() const { return PurchasesThisRound; }

protected:
    UFUNCTION(Server, Reliable)
    void Server_ActivateCurrentGum();

    UFUNCTION()
    void OnRep_CurrentActiveGum();

    UFUNCTION()
    void OnRep_RemainingUses();

protected:
    UPROPERTY(ReplicatedUsing = OnRep_CurrentActiveGum, BlueprintReadOnly, Category = "GobbleGum|State")
    TObjectPtr<UGobbleGumData> CurrentActiveGum = nullptr;

    UPROPERTY(ReplicatedUsing = OnRep_RemainingUses, BlueprintReadOnly, Category = "GobbleGum|State")
    uint8 RemainingUses = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "GobbleGum|State")
    uint8 PurchasesThisRound = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|Loadout")
    TArray<TObjectPtr<UGobbleGumData>> EquippedGums;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UGobbleGumData>> RemainingDeck;

    float LastClientRequestTime = -1.0f;
    float LastServerActivationTime = -1.0f;
    const float ActivationCooldown = 0.4f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "GobbleGum|State")
    bool bHasBoughtThisRound = false;

    UPROPERTY(Transient)
    TObjectPtr<UGobbleGumAction> ActiveActionInstance = nullptr;

public:
    UPROPERTY(BlueprintAssignable, Category = "GobbleGum|Events")
    FOnCurrentGumChanged OnCurrentGumChanged;

    UPROPERTY(BlueprintAssignable, Category = "GobbleGum|Events")
    FOnUsesRemainingChanged OnUsesRemainingChanged;

    bool ShouldPreservePerksOnDown() const { return PreservePerksCount > 0; }

    void ModifyPreservePerksCount(int32 Delta);

    // لە بەشی public:
    UFUNCTION(BlueprintAuthorityOnly, Category = "GobbleGum")
    void Server_StartEatingGum(UGobbleGumData* GumToEat);

    UFUNCTION(BlueprintAuthorityOnly, Category = "GobbleGum")
    void Server_CancelEatingGum();

    UFUNCTION(BlueprintPure, Category = "GobbleGum")
    bool IsEatingGum() const { return bIsEatingGum; }

    // لە بەشی protected:
    void Server_FinishEatingGum();

    UFUNCTION()
    void OnRep_IsEatingGum();

    UPROPERTY(ReplicatedUsing = OnRep_IsEatingGum, BlueprintReadOnly, Category = "GobbleGum|State")
    bool bIsEatingGum = false;

    // ئەنیمەیشنی خواردنی گۆبڵگەم (ئەگەر بەتاڵیش بێت، سیستەمەکە بەپێی EatDuration هەر کار دەکات)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|Animation")
    TObjectPtr<UAnimMontage> EatGumMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|Animation")
    TObjectPtr<USoundBase> EatGumSound = nullptr;

    // ماوەی خواردنی گەمەکە بە چرکە (دەبێت لەگەڵ درێژیی ئەنیمەیشنەکەدا بگونجێت)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|Animation", meta = (ClampMin = "0.1"))
    float EatDuration = 1.2f;

    UPROPERTY(Transient)
    TObjectPtr<UGobbleGumData> PendingGumToEat = nullptr;

    FTimerHandle TimerHandle_EatGum;

private:
    uint8 PreservePerksCount = 0;
};