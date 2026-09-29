#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h"
#include "DamageableInterface.h"
#include "Grenade.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class USoundBase;
class UNiagaraSystem; // 🟢 UE5 Standard: بەکارهێنانی Niagara

UCLASS()
class LASTSTANDLEGACY_API AGrenade : public AActor, public IDamageableInterface
{
    GENERATED_BODY()

public:
    AGrenade();

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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

    // FX Assets (UE5 Standard)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|FX")
    TObjectPtr<USoundBase> ExplosionSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grenade|FX")
    TObjectPtr<UNiagaraSystem> ExplosionVFX; // 🟢 Niagara

    UPROPERTY(ReplicatedUsing = OnRep_HasExploded)
    uint8 bHasExploded : 1; // 🟢 State Replication بۆ گەرەنتی کردنی پەخشی FX

    UPROPERTY(ReplicatedUsing = OnRep_InitialVelocity)
    FVector_NetQuantize InitialVelocity;

    FTimerHandle FuseTimerHandle;

    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

    void Explode();

    UFUNCTION()
    void OnRep_HasExploded();

    UFUNCTION()
    void OnRep_InitialVelocity();

public:
    void SetFuseDuration(float NewDuration);
    void InitVelocity(const FVector& InVelocity, APawn* InInstigator); // 🟢 وەرگرتنی Instigator

    virtual bool CanReceiveWeaponDamage() const override { return !bHasExploded; }
};