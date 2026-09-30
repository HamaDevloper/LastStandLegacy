#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h" // ١. زیاکراوە بۆ FVector_NetQuantize
#include "Grenade.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class USoundBase;
class UParticleSystem;

UCLASS()
class LASTSTANDLEGACY_API AGrenade : public AActor
{
    GENERATED_BODY()

public:
    AGrenade();

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USphereComponent> CollisionComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MeshComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UProjectileMovementComponent> ProjectileMovementComp;

    // Grenade Config Properties
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|Config")
    float FuseDuration = 3.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|Config")
    float BaseDamage = 500.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|Config")
    float DamageRadius = 500.0f;

    // FX Assets
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|FX")
    TObjectPtr<USoundBase> ExplosionSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|FX")
    TObjectPtr<UParticleSystem> ExplosionVFX;

    uint8 bHasExploded : 1;

    UPROPERTY(ReplicatedUsing = OnRep_InitialVelocity)
    FVector_NetQuantize InitialVelocity;

    FTimerHandle FuseTimerHandle;

    void Explode();
   
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayExplosionFX(FVector_NetQuantize ExplosionLocation);

    UFUNCTION()
    void OnRep_InitialVelocity();

public:
    void SetFuseDuration(float NewDuration);
    void InitVelocity(const FVector& InVelocity);
};