#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "LastStandLegacyGameState.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnRoundChanged, int32);

UCLASS()
class LASTSTANDLEGACY_API ALastStandLegacyGameState : public AGameState
{
    GENERATED_BODY()

public:
    ALastStandLegacyGameState();

    // 🚨 تێبینی: UPROPERTY لێکراوەتەوە چونکە Native C++ Delegateـە
    FOnRoundChanged OnRoundChangedDelegate;

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
    // ── سیستەمی ڕاوەند ──
    void SetCurrentRound(int32 NewRound);
    int32 GetCurrentRound() const { return CurrentRound; }

protected:
    UPROPERTY(ReplicatedUsing = OnRep_CurrentRound, BlueprintReadOnly, Category = "Zombie|Round")
    int32 CurrentRound = 1;

    UFUNCTION()
    void OnRep_CurrentRound();

public:
    // ── ئەبیلیتی و پاوەرەکان ──
    UPROPERTY(ReplicatedUsing = OnRep_GlobalBulletStorm, BlueprintReadOnly, Category = "Abilities")
    bool bIsGlobalBulletStormActive = false;

    UPROPERTY(ReplicatedUsing = OnRep_Adrenaline, BlueprintReadOnly, Category = "Abilities")
    bool bIsAdrenalineActive = false;

    UPROPERTY(ReplicatedUsing = OnRep_DoublePoints, BlueprintReadOnly, Category = "Abilities")
    bool bIsDoublePointsActive = false;

    UPROPERTY(ReplicatedUsing = OnRep_InstaKill, BlueprintReadOnly, Category = "Abilities")
    bool bHasInstaKill = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "GameState|Power")
    bool bIsPowerOn = true;

    // ئەمە لە GameModeـەوە دەیکەیت بە true ئەگەر تەنها ١ یاریزان لە سێرڤەرەکە بوو
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "GameState|Match")
    bool bIsSoloMatch = false;

    void StartTeamAdrenaline(float Duration);
    bool IsTeamAdrenalineActive() const { return bIsAdrenalineActive; }

    bool GetInstaKill() const { return bHasInstaKill; }

    void StartGlobalBulletStorm(float Duration);
    void StartDoublePoints(float Duration);
    void StartinstaKill(float Duration);

    // ── لیستی ئامانجەکان (0-CPU Overhead) ──
    UPROPERTY()
    TArray<APawn*> ValidTargets;

    void RegisterTarget(APawn* NewTarget);
    void UnregisterTarget(APawn* TargetToRemove);

protected:
    void EndDoublePoints();
    void EndGlobalBulletStorm();
    void EndInstaKill();
    void EndTeamAdrenaline();

    UFUNCTION()
    void OnRep_GlobalBulletStorm();

    UFUNCTION()
    void OnRep_DoublePoints();

    UFUNCTION()
    void OnRep_InstaKill();

    UFUNCTION()
    void OnRep_Adrenaline();

private:
    FTimerHandle BulletStormTimer;
    FTimerHandle DoublePointTimer;
    FTimerHandle InstaKillTimer;
    FTimerHandle AdrenalineTimerHandle;
};