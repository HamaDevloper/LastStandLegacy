#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h"
#include "DamageableInterface.h"
#include "MonkeyBomb.generated.h"

#define ECC_Bullet ECC_GameTraceChannel1

class USphereComponent;
class UStaticMeshComponent;
class UAudioComponent;
class UProjectileMovementComponent;
class USoundBase;
class UNiagaraSystem; // 🟢 UE5 Standard: بەکارهێنانی Niagara

UCLASS()
class LASTSTANDLEGACY_API AMonkeyBomb : public AActor, public IDamageableInterface
{
    GENERATED_BODY()

public:
    AMonkeyBomb();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MeshComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UAudioComponent> AttractionAudioComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UProjectileMovementComponent> ProjectileMovementComp;

    // Config
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MonkeyBomb|Config")
    float AttractionRadius = 2000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MonkeyBomb|Config")
    float FuseDuration = 8.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MonkeyBomb|Config")
    float BaseDamage = 500.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MonkeyBomb|Config")
    float DamageRadius = 600.0f;

    // FX Assets (UE5 Standard)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MonkeyBomb|FX")
    TObjectPtr<USoundBase> AttractionSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MonkeyBomb|FX")
    TObjectPtr<USoundBase> ExplosionSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MonkeyBomb|FX")
    TObjectPtr<UNiagaraSystem> ExplosionVFX; // 🟢 Niagara System

    UPROPERTY(ReplicatedUsing = OnRep_AttractionActivated)
    uint8 bAttractionActivated : 1;

    UPROPERTY(ReplicatedUsing = OnRep_HasExploded)
    uint8 bHasExploded : 1; // 🟢 State Replication بۆ ڕێگری لە Race Condition

    UPROPERTY(ReplicatedUsing = OnRep_InitialVelocity)
    FVector_NetQuantize InitialVelocity;

    FTimerHandle FuseTimerHandle;

    UFUNCTION()
    void OnRep_AttractionActivated();

    UFUNCTION()
    void OnRep_HasExploded();

    UFUNCTION()
    void OnProjectileStopped(const FHitResult& ImpactResult);

    void ActivateAttraction();
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
    void Explode();

    UFUNCTION()
    void OnRep_InitialVelocity();

    void InternalStopMovement();

public:
    void InitVelocity(const FVector& InVelocity, APawn* InInstigator);

    virtual bool CanReceiveWeaponDamage() const override { return !bHasExploded; }
};