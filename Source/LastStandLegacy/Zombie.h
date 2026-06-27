#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Zombie.generated.h"

class AAIController;
class ALastStandLegacyGameState;

DECLARE_DELEGATE_TwoParams(FOnZombieDeath, AZombie*, AController*);

UCLASS()
class AZombie : public ACharacter
{
    GENERATED_BODY()

public:
    AZombie();

    FOnZombieDeath OnZombieDeath;

    void SetStatsForRound(int32 CurrentRound);

    virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser) override;

protected:
    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    FTimerHandle ChaseTimerHandle;
    FTimerHandle AttackTimerHandle;

    UPROPERTY()
    TObjectPtr<AAIController> CachedAIController;

    UPROPERTY()
    TObjectPtr<ALastStandLegacyGameState> CachedGS;

    UPROPERTY()
    TObjectPtr<APawn> CurrentTarget;

protected:
    UPROPERTY(ReplicatedUsing = OnRep_IsDead)
    bool bIsDead = false;

    UPROPERTY(Replicated, BlueprintReadOnly)
    float Health = 0.f;

    UPROPERTY(Replicated)
    float MaxHealth = 0.f;

    UPROPERTY(EditDefaultsOnly, Category = "Zombie|Stats")
    float BaseHealth = 150.f;

    UPROPERTY(EditDefaultsOnly, Category = "Zombie|Stats")
    float AttackDamage = 25.f;

    UPROPERTY(EditDefaultsOnly, Category = "Zombie|Stats")
    float AttackDistance = 120.f;

    float AttackDistanceSq = 0.f;

    void UpdateNearestTarget();
    void CheckAttackRange();

    UFUNCTION()
    void OnRep_IsDead();

public:
    void Die(AController* KillerController);
    bool IsDead() const { return bIsDead; }
};