#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Zombie.generated.h"

class AAIController;
class ALastStandLegacyGameState;
class UZombieDirectorSubsystem;

// گەر ئەم Delegateـەت نەبوو پێشتر، زیادی بکە، بەڵام وادیارە لە کۆدە کۆنەکەتدا هەبووە
DECLARE_DELEGATE_TwoParams(FOnZombieDeathSignature, AZombie*, AController*);

UCLASS()
class LASTSTANDLEGACY_API AZombie : public ACharacter
{
    GENERATED_BODY()

public:
    AZombie();

    virtual void PossessedBy(AController* NewController) override;
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

    void SetStatsForRound(int32 CurrentRound);

    // گۆڕاوە نوێیەکان بۆ Subsystem
    UPROPERTY(Transient)
    APawn* CurrentTarget = nullptr;

    UPROPERTY(Transient)
    FVector LastTargetLocation = FVector::ZeroVector;

    // فەنکشنی نوێی لێدان (Anti-Ghost Hit) کە دەبێت لە ئەنیمەیشنەوە بانگ بکرێت
    UFUNCTION(BlueprintCallable, Category = "Zombie|Combat")
    void ExecuteMeleeHit();

    FOnZombieDeathSignature OnZombieDeath;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override; // گرنگە بۆ سەلامەتی سێرڤەر
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_IsDead();

public:
    UPROPERTY(ReplicatedUsing = OnRep_IsDead, BlueprintReadOnly, Category = "Zombie|State")
    bool bIsDead = false;

    UPROPERTY(EditDefaultsOnly, Category = "Zombie|Stats")
    float BaseHealth = 150.f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Zombie|Stats")
    float MaxHealth;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Zombie|Stats")
    float Health;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Stats")
    float MaxHealthZombieReach = 60000.f;

    UPROPERTY(EditDefaultsOnly, Category = "Zombie|Stats")
    float AttackDamage = 50.f;

    UPROPERTY(EditDefaultsOnly, Category = "Zombie|Stats")
    float AttackDistance = 150.f;

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Stats")
    float MinWalkSpeed = 200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Stats")
    float MaxBaseWalkSpeed = 500.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Stats")
    float AbsoluteMaxSpeed = 650.f;

    float AttackDistanceSq;

public:
    void Die(AController* KillerController);

    bool IsDead() const { return bIsDead; }

    UPROPERTY(Transient)
    TObjectPtr<AAIController> CachedAIController = nullptr;

private:
    UPROPERTY()
    TObjectPtr<ALastStandLegacyGameState> CachedGS = nullptr;

    UPROPERTY()
    TObjectPtr<UCharacterMovementComponent> CachedMovement = nullptr;

    UPROPERTY()
    TObjectPtr<UZombieDirectorSubsystem> CachedDirector = nullptr;

    bool bRegisteredWithDirector = false;


public:
    float NextTargetSearchTime = 0.f;

};