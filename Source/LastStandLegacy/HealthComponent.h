#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

class AHama;
class UHamaComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeathDelegate);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LASTSTANDLEGACY_API UHealthComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHealthComponent();

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(ReplicatedUsing = OnRep_CurrentHealth, EditAnywhere, BlueprintReadOnly, Category = "Health")
    float CurrentHealth = 100.f;

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Health")
    float MaxHealth = 100.f;

    // 🔒 تەنها لەسەر سێرڤەر بەکاردێت بۆ ڕێگری لەوەی دوو یاریزان هاوکات Reviveی بکان (Unreplicated)
    bool bIsBeingRevived = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
    float HealthGenerateDelay = 3.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
    float HealthTickGenerate = 0.1f;

    UFUNCTION()
    void OnRep_CurrentHealth(float OldHealth);

public:
    void ApplyDamage(float Amount, AActor* DamageCauser);
    void UpgradeHealth(float Amount);
    void Revive();

    bool IsDowned() const;

    FORCEINLINE bool IsBeingRevived() const { return bIsBeingRevived; }
    FORCEINLINE void SetBeingRevived(bool bState) { bIsBeingRevived = bState; }

    UPROPERTY(BlueprintAssignable)
    FOnDeathDelegate OnDeath;

protected:
    void DownPlayer();
    void RegenerateHealth();
    void HandlePlayerDeath();

private:
    UPROPERTY()
    TObjectPtr<AHama> OwnerCharacter;

    UPROPERTY()
    TObjectPtr<UHamaComponent> OwnerComponent;

    FTimerHandle RegenerateHealthTimer;
    FTimerHandle DownTimerHandle;
    FTimerHandle QuickReviveTimerHandle;
};