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

    UFUNCTION()
    void OnRep_GlobalBulletStorm();

    // زیادکراوەکان بۆ کۆنتڕۆڵکردنی تواناکە
    void StartGlobalBulletStorm(float Duration);
    void EndGlobalBulletStorm();

private:
    FTimerHandle BulletStormTimerHandle;
};