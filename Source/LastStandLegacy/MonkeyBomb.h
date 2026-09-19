#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h" // ١. زیاکراوە بۆ پشتبەستن بە FVector_NetQuantize
#include "MonkeyBomb.generated.h"

#define ECC_Bullet ECC_GameTraceChannel1

class USphereComponent;
class UStaticMeshComponent;
class UAudioComponent;
class UProjectileMovementComponent;
class USoundBase;
class UParticleSystem;

UCLASS()
class LASTSTANDLEGACY_API AMonkeyBomb : public AActor
{
    GENERATED_BODY()

public:
    AMonkeyBomb();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;

    // Components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USphereComponent> CollisionComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MeshComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UAudioComponent> AttractionAudioComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UProjectileMovementComponent> ProjectileMovementComp;

    // Config Properties
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MonkeyBomb|Config")
    float AttractionRadius = 2000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MonkeyBomb|Config")
    float FuseDuration = 8.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MonkeyBomb|Config")
    float BaseDamage = 500.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MonkeyBomb|Config")
    float DamageRadius = 600.0f;

    // FX Assets
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MonkeyBomb|FX")
    TObjectPtr<USoundBase> AttractionSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MonkeyBomb|FX")
    TObjectPtr<USoundBase> ExplosionSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MonkeyBomb|FX")
    TObjectPtr<UParticleSystem> ExplosionVFX;

    UPROPERTY(ReplicatedUsing = OnRep_AttractionActivated)
    uint8 bAttractionActivated : 1;

    uint8 bHasExploded : 1;

    FTimerHandle FuseTimerHandle;

    UFUNCTION()
    void OnRep_AttractionActivated();

    UFUNCTION()
    void OnProjectileStopped(const FHitResult& ImpactResult);

    void ActivateAttraction();
    void Explode();

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayExplosionFX(FVector_NetQuantize ExplosionLocation);
};