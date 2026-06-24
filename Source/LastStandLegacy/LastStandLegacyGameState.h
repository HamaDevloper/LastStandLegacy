#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "LastStandLegacyGameState.generated.h"

UCLASS()
class LASTSTANDLEGACY_API ALastStandLegacyGameState : public AGameState
{
    GENERATED_BODY()

public:
    ALastStandLegacyGameState();

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
    UPROPERTY(ReplicatedUsing = OnRep_GlobalBulletStorm, BlueprintReadOnly, Category = "Abilities")
    bool bIsGlobalBulletStormActive = false;

    UPROPERTY(ReplicatedUsing = OnRep_DoublePoints, BlueprintReadOnly, Category = "Abilities")
    bool bIsDoublePointsActive = false;

    UPROPERTY(ReplicatedUsing = OnRep_InstaKill, BlueprintReadOnly, Category = "Abilities")
    bool bHasInstaKill = false;

    UFUNCTION()
    void OnRep_GlobalBulletStorm();

    UFUNCTION()
    void OnRep_DoublePoints();

    UFUNCTION()
    void OnRep_InstaKill();


    bool GetInstaKill() const { return bHasInstaKill; }

    // زیادکراوەکان بۆ کۆنتڕۆڵکردنی تواناکە
    void StartGlobalBulletStorm(float Duration);
    void StartDoublePoints(float Duration);
    void StartinstaKill(float Duration);

protected:
    void EndDoublePoints();
    void EndGlobalBulletStorm();
    void EndInstaKill();

private:
    FTimerHandle BulletStormTimer;
    FTimerHandle DoublePointTimer;
    FTimerHandle InstaKillTimer;
};