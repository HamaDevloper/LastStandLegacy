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

    // CurrentTarget تەنیا لەسەر سێرڤەرە و کڵایەنت پێویستی پێی نییە (Replication لابرا)
    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<AActor> CurrentTarget;

    UPROPERTY(Transient)
    FVector LastTargetLocation = FVector::ZeroVector;

    UFUNCTION(BlueprintCallable, Category = "Zombie|Combat")
    void ExecuteMeleeHit();

    void TryAttack();

    FOnZombieDeathSignature OnZombieDeath;

    void InitializeForPool();
    void ActivateFromPool(const FVector& SpawnLocation, const FRotator& SpawnRotation, int32 CurrentRound);
    void ReturnToPool();

    FORCEINLINE bool IsPoolAvailable() const { return PoolState == EZombiePoolState::Pooled; }
    FORCEINLINE bool IsDead() const { return bIsDead; }
    FORCEINLINE bool IsAttacking() const { return bIsAttacking; }

    UPROPERTY(Transient)
    TObjectPtr<AAIController> CachedAIController = nullptr;

    float NextTargetSearchTime = 0.0f;
    float NextForceRepathTime = 0.f;
    float NextRepathAllowedTime = 0.f; // بەربەستی کاتی بۆ ڕێگریکردن لە لافاوی Repath

    void ApplyServerLOD(uint8 Tier);
    void Die(AController* KillerController);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void OnRep_ReplicatedMovement() override;

    UFUNCTION()
    void OnRep_PoolState();

    UFUNCTION()
    void OnRep_SelectedIndex();

    UFUNCTION()
    void OnRep_AttackPlayCount();

    void ApplySelectedMesh();
    void ApplyStateVisualsAndPhysics(EZombiePoolState NewState);
    void FinalizePoolActivationVisuals();

    UFUNCTION()
    void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    virtual bool CanReceiveWeaponDamage() const override { return !bIsDead; }

protected:
    UPROPERTY(ReplicatedUsing = OnRep_PoolState)
    EZombiePoolState PoolState = EZombiePoolState::Alive;

    // bIsDead لە ڕیپلیکەیشن دەرهێنرا؛ کڵایەنت لە PoolState دەیزانێت
    UPROPERTY(BlueprintReadOnly, Category = "Zombie|State")
    bool bIsDead = false;

    uint8 ServerLODTier = 255;

    UPROPERTY(ReplicatedUsing = OnRep_AttackPlayCount)
    uint8 AttackPlayCount = 0;

    UPROPERTY(Replicated)
    uint8 SelectedAttackMontageIndex = 0;

    bool bIsAttacking = false;

    UPROPERTY(EditDefaultsOnly, Category = "Zombie|Combat")
    TArray<TObjectPtr<UAnimMontage>> AttackMontages;

    UPROPERTY(EditDefaultsOnly, Category = "Zombie|Combat")
    float AttackCooldown = 1.2f;

    float NextAllowedAttackTime = 0.f;

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Stats")
    float MinWalkSpeed = 200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Stats")
    float MaxBaseWalkSpeed = 500.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Stats")
    float AbsoluteMaxSpeed = 650.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zombie|Visuals")
    TArray<TSoftObjectPtr<USkeletalMesh>> MeshToSelect;

public:
    UPROPERTY(EditDefaultsOnly, Category = "Zombie|Stats")
    float AttackDistance = 150.f;

    float AttackDistanceSq;

private:
    UPROPERTY()
    TObjectPtr<ALastStandLegacyGameState> CachedGS = nullptr;

    UPROPERTY()
    TObjectPtr<UCharacterMovementComponent> CachedMovement = nullptr;

    UPROPERTY()
    TObjectPtr<UZombieDirectorSubsystem> CachedDirector = nullptr;

    bool bRegisteredWithDirector = false;

    FVector CachedMeshRelativeLocation = FVector::ZeroVector;
    FRotator CachedMeshRelativeRotation = FRotator::ZeroRotator;
    FName CachedMeshCollisionProfile = NAME_None;
    TEnumAsByte<ECollisionChannel> CachedMeshObjectType = ECC_Pawn;
    FCollisionResponseContainer CachedMeshCollisionResponses;
    ECollisionEnabled::Type CachedMeshCollisionEnabled = ECollisionEnabled::QueryOnly;
    ECollisionEnabled::Type CachedCapsuleCollisionEnabled = ECollisionEnabled::QueryAndPhysics;

    FTimerHandle TimerHandle_ReturnToPool;
    FTimerHandle TimerHandle_RevealMesh;
    FTimerHandle TimerHandle_Dormant;

    void GoDormant();
};