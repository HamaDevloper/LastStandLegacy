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

    // 🚀 لابردنی ڕەقە ژمارەکان و دانانی باری ڕاستەقینە بۆ فرەیاریزان
    UPROPERTY(ReplicatedUsing = OnRep_CurrentHealth, EditAnywhere, BlueprintReadOnly, Category = "Health")
    float CurrentHealth = 100.f;

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Health")
    float MaxHealth = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
    float HealthGenerateDelay = 3.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
    float HealthTickGenerate = 0.1f;

    UFUNCTION()
    void OnRep_CurrentHealth(float OldHealth);

public:
    // 🚀 فەنکشنە تازەکە لەبری GetDamageـی کۆن
    void ApplyDamage(float Amount, AActor* DamageCauser);
    void UpgradeHealth(float Amount);
    void Revive();

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
    FTimerHandle ReviveTimerHandle;
    FTimerHandle DownTimerHandle;
};