#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MonkeyBomb.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UAudioComponent;
class USoundBase;
class UParticleSystem;

UCLASS()
class LASTSTANDLEGACY_API AMonkeyBomb : public AActor
{
    GENERATED_BODY()

public:
    AMonkeyBomb();

protected:
    virtual void BeginPlay() override;

    // --- Components ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USphereComponent> CollisionComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MeshComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UProjectileMovementComponent> ProjectileMovementComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UAudioComponent> AttractionAudioComp;

    // --- Config & Tuning ---
    UPROPERTY(EditDefaultsOnly, Category = "Monkey Properties")
    float AttractionRadius = 2500.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Monkey Properties")
    float FuseDuration = 8.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Monkey Properties|Damage")
    float BaseDamage = 1000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Monkey Properties|Damage")
    float DamageRadius = 600.0f;

    // --- FX & Audio ---
    UPROPERTY(EditDefaultsOnly, Category = "Monkey Properties|Effects")
    TObjectPtr<USoundBase> AttractionSound;

    UPROPERTY(EditDefaultsOnly, Category = "Monkey Properties|Effects")
    TObjectPtr<USoundBase> ExplosionSound;

    UPROPERTY(EditDefaultsOnly, Category = "Monkey Properties|Effects")
    TObjectPtr<UParticleSystem> ExplosionVFX;

    // --- Collision & Logic ---
    UFUNCTION()
    void OnBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity);

    void ActivateAttraction();
    void Explode();

    // --- Net Multicasts ---
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlaySound();

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayExplosionFX();

private:
    FTimerHandle FuseTimerHandle;
};