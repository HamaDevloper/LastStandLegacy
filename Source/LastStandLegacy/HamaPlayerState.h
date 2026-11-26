#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "HamaAbilityComponent.h"
#include "HamaPlayerState.generated.h"

DECLARE_DELEGATE_OneParam(FOnPointsChangedSignature, int32);
DECLARE_DELEGATE_OneParam(FOnKillsChangedSignature, int32);
DECLARE_DELEGATE_OneParam(FOnDownsChangedSignature, int32);
DECLARE_DELEGATE_OneParam(FOnRevievsChangedSignature, int32);

class ABaseWeapon;

UCLASS()
class LASTSTANDLEGACY_API AHamaPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    AHamaPlayerState();

    FOnPointsChangedSignature OnPointsChanged;
    FOnKillsChangedSignature OnKillsChanged;
    FOnDownsChangedSignature OnDownsChanged;
    FOnRevievsChangedSignature OnRevivesChanged;

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(ReplicatedUsing = OnRep_Points, BlueprintReadOnly, Category = "Player State")
    int32 Points;

    UPROPERTY(ReplicatedUsing = OnRep_Kills, BlueprintReadOnly, Category = "Player State")
    int32 Kills;

    UPROPERTY(ReplicatedUsing = OnRep_Downed, BlueprintReadOnly, Category = "Player State")
    int32 Downes;

    UPROPERTY(ReplicatedUsing = OnRep_Revived, BlueprintReadOnly, Category = "Player State")
    int32 Revives;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player State")
    int32 MaxPointToEarn = 1000000;

    UPROPERTY(ReplicatedUsing = OnRep_AssignedRole, BlueprintReadOnly, Category = "Player State")
    EHamaAbilityType AssignedRole = EHamaAbilityType::None;

    UFUNCTION()
    void OnRep_Points();

    UFUNCTION()
    void OnRep_Kills();

    UFUNCTION()
    void OnRep_AssignedRole();

    UFUNCTION()
    void OnRep_Downed();

    UFUNCTION()
    void OnRep_Revived();

public:
    void SetPoints(int32 Amount);

    void AddPoints(int32 Amount, bool bAffectedByMultiplier = true);

    void RemovePoints(int32 Amount);

    int32 GetPoints() const { return Points; }

    void AddKills(int32 Amount);

    int32 GetKills() const { return Kills; }

    void AddDowns(int32 Amount);

    int32 GetDowns() const { return Downes; }

    void AddRevives(int32 Amount);

    int32 GetRevievs() const { return Revives; }

    void SetAssignedRole(EHamaAbilityType NewRole);

    EHamaAbilityType GetAssignedRole() const { return AssignedRole; }

    float GetExactPing() const { return GetPingInMilliseconds(); }


public:
    void SetLegacyKeeperArmed(bool bArmed);
    bool IsLegacyKeeperArmed() const { return bIsLegacyKeeperArmed; }

    void SaveLoadoutForLegacyKeeper(const TArray<TSubclassOf<ABaseWeapon>>& WeaponsToSave);
    bool HasPendingLegacyLoadout() const { return bHasPendingLegacyLoadout && SavedLegacyLoadout.Num() > 0; }
    TArray<TSubclassOf<ABaseWeapon>> ConsumePendingLegacyLoadout();

private:
    bool bIsLegacyKeeperArmed = false;
    bool bHasPendingLegacyLoadout = false;
    TArray<TSubclassOf<ABaseWeapon>> SavedLegacyLoadout;
};