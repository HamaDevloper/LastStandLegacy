#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/Core/PushModel/PushModel.h"
#include "GobbleGumTypes.h"
#include "GobbleGumComponent.generated.h"

class UGobbleGumData;
class ACharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCurrentGumChanged, const UGobbleGumData*, NewGum);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUsesRemainingChanged, uint8, UsesLeft);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LASTSTANDLEGACY_API UGobbleGumComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGobbleGumComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|Loadout")
    TArray<TObjectPtr<UGobbleGumData>> EquippedGums;

    UPROPERTY()
    TArray<TObjectPtr<UGobbleGumData>> RemainingDeck;

    float LastActivationTime = 0.0f;
    const float ActivationCooldown = 0.4f;

public:
    UPROPERTY(BlueprintAssignable, Category = "GobbleGum|Events")
    FOnCurrentGumChanged OnCurrentGumChanged;

    UPROPERTY(BlueprintAssignable, Category = "GobbleGum|Events")
    FOnUsesRemainingChanged OnUsesRemainingChanged;
};