#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DamageableInterface.h"
#include "Navigation/PathFollowingComponent.h"
#include "Zombie.generated.h"

class AAIController;
class ALastStandLegacyGameState;
class UZombieDirectorSubsystem;

DECLARE_DELEGATE_TwoParams(FOnZombieDeathSignature, AZombie*, AController*);


UENUM(BlueprintType)
enum class EZombiePoolState : uint8
{
    Pooled UMETA(DisplayName = "In Pool (Inactive)"),
    Alive  UMETA(DisplayName = "Alive (Active)"),
    Dead   UMETA(DisplayName = "Dead (Ragdoll)")
};

UCLASS()
class LASTSTANDLEGACY_API AZombie : public ACharacter, public IDamageableInterface
{
    GENERATED_BODY()

public:
    AZombie();

    virtual void PossessedBy(AController* NewController) override;
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

    void SetStatsForRound(int32 CurrentRound);

    UPROPERTY(BlueprintReadOnly, Replicated)
    TObjectPtr<AActor> CurrentTarget;

    UPROPERTY(Transient)
    FVector LastTargetLocation = FVector::ZeroVector;

    // فەنکشنی نوێی لێدان (Anti-Ghost Hit) کە دەبێت لە ئەنیمەیشنەوە بانگ بکرێت
    UFUNCTION(BlueprintCallable, Category = "Zombie|Combat")
    void ExecuteMeleeHit();

    FOnZombieDeathSignature OnZombieDeath;

    void InitializeForPool();
    void ActivateFromPool(const FVector& SpawnLocation, const FRotator& SpawnRotation, int32 CurrentRound);
    void ReturnToPool();

    FORCEINLINE bool IsPoolAvailable() const { return PoolState == EZombiePoolState::Pooled; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override; // گرنگە بۆ سەلامەتی سێرڤەر
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_IsDead();

    UFUNCTION()
    void OnRep_SelectedIndex();

    void ApplySelectedMesh();

protected:

    UPROPERTY(ReplicatedUsing = OnRep_PoolState)
    EZombiePoolState PoolState = EZombiePoolState::Alive;

    UPROPERTY(ReplicatedUsing = OnRep_IsDead, BlueprintReadOnly, Category = "Zombie|State")
    bool bIsDead = false;

    UPROPERTY(EditDefaultsOnly, Category = "Zombie|Stats")
    float BaseHealth = 150.f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Zombie|Stats")
    float MaxHealth;

    UPROPERTY(BlueprintReadOnly, Category = "Zombie|Stats")
    float Health;

    UPROPERTY(ReplicatedUsing = OnRep_SelectedIndex, BlueprintReadOnly, Category = "Zombie|Stats")
    uint8 MeshIndexSelected = 255;

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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zombie|Visuals")
    TArray<TSoftObjectPtr<USkeletalMesh>> MeshToSelect;

    float AttackDistanceSq;

public:
    void Die(AController* KillerController);

    UPROPERTY(Transient)
    TObjectPtr<AAIController> CachedAIController = nullptr;

    FORCEINLINE bool IsDead() const { return bIsDead; }

private:
    UPROPERTY()
    TObjectPtr<ALastStandLegacyGameState> CachedGS = nullptr;
     
    UPROPERTY()
    TObjectPtr<UCharacterMovementComponent> CachedMovement = nullptr;

    UPROPERTY()
    TObjectPtr<UZombieDirectorSubsystem> CachedDirector = nullptr;

    bool bRegisteredWithDirector = false;

    UFUNCTION()
    void OnRep_PoolState();

    void ApplyStateVisualsAndPhysics(EZombiePoolState NewState);

    virtual bool CanReceiveWeaponDamage() const override { return !bIsDead; }
public:
    float NextTargetSearchTime = 0.0f;
    float NextForceRepathTime = 0.f;

private:
    FVector CachedMeshRelativeLocation = FVector::ZeroVector;
    FRotator CachedMeshRelativeRotation = FRotator::ZeroRotator;
    FName CachedMeshCollisionProfile = NAME_None;
    ECollisionEnabled::Type CachedMeshCollisionEnabled = ECollisionEnabled::QueryOnly;
    ECollisionEnabled::Type CachedCapsuleCollisionEnabled = ECollisionEnabled::QueryAndPhysics;

    FTimerHandle TimerHandle_ReturnToPool;
};