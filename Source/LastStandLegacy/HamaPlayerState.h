#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "HamaAbilityComponent.h"
#include "HamaPlayerState.generated.h"

DECLARE_DELEGATE_OneParam(FOnPointsChangedSignature, int32);
DECLARE_DELEGATE_OneParam(FOnKillsChangedSignature, int32);

UCLASS()
class LASTSTANDLEGACY_API AHamaPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    AHamaPlayerState();

    FOnPointsChangedSignature OnPointsChanged;
    FOnKillsChangedSignature OnKillsChanged;

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(ReplicatedUsing = OnRep_Points, BlueprintReadOnly, Category = "Player State")
    int32 Points;

    UPROPERTY(ReplicatedUsing = OnRep_Kills, BlueprintReadOnly, Category = "Player State")
    int32 Kills;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player State")
    int32 MaxPointToEarn = 1000000;

    // گۆڕدرا بۆ ReplicatedUsing بۆ ئەوەی کڵایەنتەکان بتوانن مەشەکەیان ئەپدەیت بکەن
    UPROPERTY(ReplicatedUsing = OnRep_AssignedRole, BlueprintReadOnly, Category = "Player State")
    EHamaAbilityType AssignedRole = EHamaAbilityType::None;

    UFUNCTION()
    void OnRep_Points();

    UFUNCTION()
    void OnRep_Kills();

    UFUNCTION()
    void OnRep_AssignedRole();

public:
    UFUNCTION(BlueprintCallable, Category = "Economy")
    void SetPoints(int32 NewPoints);

    UFUNCTION(BlueprintCallable, Category = "Player State")
    void AddPoints(int32 Amount);

    UFUNCTION(BlueprintCallable, Category = "Player State")
    void RemovePoints(int32 Amount);

    UFUNCTION(BlueprintCallable, Category = "Player State")
    void AddKills(int32 Amount);

    UFUNCTION(BlueprintCallable, Category = "Player State")
    int32 GetPoints() const { return Points; }

    UFUNCTION(BlueprintCallable, Category = "Player State")
    int32 GetKills() const { return Kills; }

    UFUNCTION(Client, Unreliable)
    void Client_OnPointGained(int32 NewPoints);

    UFUNCTION(Client, Unreliable)
    void Client_OnKillGained(int32 NewKill);

    UFUNCTION(BlueprintCallable, Category = "Player State")
    void SetAssignedRole(EHamaAbilityType NewRole);

    UFUNCTION(BlueprintCallable, Category = "Player State")
    EHamaAbilityType GetAssignedRole() const { return AssignedRole; }

    float GetExactPing() const { return GetPingInMilliseconds(); }
};