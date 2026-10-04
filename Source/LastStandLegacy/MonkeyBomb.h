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
class UNiagaraSystem;

UCLASS()
class LASTSTANDLEGACY_API AMonkeyBomb : public AActor, public IDamageableInterface
{
    GENERATED_BODY()

public:
    AMonkeyBomb();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USphereComponent> CollisionComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MeshComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UAudioComponent> AttractionAudioComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UProjectileMovementComponent> ProjectileMovementComp;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MonkeyBomb|Config")
    float AttractionRadius = 2000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MonkeyBomb|Config")
    float FuseDuration = 8.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MonkeyBomb|Config")
    float BaseDamage = 500.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MonkeyBomb|Config")
    float DamageRadius = 600.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MonkeyBomb|FX")
    TObjectPtr<USoundBase> AttractionSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MonkeyBomb|FX")
    TObjectPtr<USoundBase> ExplosionSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MonkeyBomb|FX")
    TObjectPtr<UNiagaraSystem> ExplosionNiagaraFX;

    UPROPERTY(ReplicatedUsing = OnRep_AttractionActivated)
    bool bAttractionActivated = false;

    UPROPERTY(ReplicatedUsing = OnRep_HasExploded)
    bool bHasExploded = false;

    UPROPERTY(Replicated)
    FVector_NetQuantize ExplosionLocation;

    UPROPERTY(ReplicatedUsing = OnRep_InitialVelocity)
    FVector_NetQuantize InitialVelocity;

    FTimerHandle FuseTimerHandle;

    UFUNCTION()
    void OnRep_AttractionActivated();

    UFUNCTION()
    void OnRep_HasExploded();

    UFUNCTION()
    void OnRep_InitialVelocity();

    UFUNCTION()
    void OnProjectileStopped(const FHitResult& ImpactResult);

    void ActivateAttraction();
    void Explode();
    void PlayExplosionFX(const FVector& Location);

public:
    void InitVelocity(const FVector& InVelocity);

    virtual bool CanReceiveWeaponDamage() const override { return !bHasExploded; }
};