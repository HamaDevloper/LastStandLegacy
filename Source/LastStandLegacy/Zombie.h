#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Zombie.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnZombieDeath, AZombie*);

UCLASS()
class LASTSTANDLEGACY_API AZombie : public ACharacter
{
    GENERATED_BODY()

public:
    AZombie();

    FOnZombieDeath OnZombieDeath;

    UFUNCTION(BlueprintCallable, Category = "Zombie | Stats")
    void SetStatsForRound(int32 CurrentRound);

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    FTimerHandle ChaseTimerHandle;
    FTimerHandle AttackTimerHandle;

    void UpdateNearestTarget();
    void CheckAttackRange();

public:
    virtual float TakeDamage(
        float DamageAmount,
        struct FDamageEvent const& DamageEvent,
        class AController* EventInstigator,
        AActor* DamageCauser) override;

    // ── Stats ──────────────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie | Stats")
    float BaseHealth = 100.f;

    // Replicated — کلاینت پێویستیەتی بۆ HealthBar UI
    UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Zombie | Stats")
    float MaxHealth;

    UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Zombie | Stats")
    float Health;

    // ReplicatedUsing — کاتی مردن Ragdoll چالاک دەبێت
    UPROPERTY(ReplicatedUsing = OnRep_IsDead,
        VisibleAnywhere, BlueprintReadOnly, Category = "Zombie | Stats")
    bool bIsDead = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie | Stats")
    int32 KillPointsValue = 100;

    // ── Combat ─────────────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, Category = "Zombie | Combat")
    float AttackDistance = 100.f;

    UPROPERTY(EditAnywhere, Category = "Zombie | Combat")
    float AttackDamage = 20.f;

    UPROPERTY()
    APawn* CurrentTarget;

    // ── Functions ──────────────────────────────────────────────────────────
    void Die(AController* KillerController);

    UFUNCTION()
    void OnRep_IsDead();
};