#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

class AHama;
class UHamaComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDownStateChangedSignature, bool /*bIsDowned*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnReviveStateChangedSignature, bool /*bIsBeingRevived*/, bool /*bIsRevivingOthers*/);
DECLARE_MULTICAST_DELEGATE(FOnDeathSignature);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UHealthComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHealthComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // --- Delegates (Multicast for Modular Architecture) ---
    FOnDownStateChangedSignature OnDownStateChanged;
    FOnReviveStateChangedSignature OnReviveStateChanged;
    FOnDeathSignature OnDeath;

    // --- Core Health & State Functions ---
    void ApplyDamage(float Amount, AActor* DamageCauser);
    void UpgradeHealth(float Amount);
    void DownPlayer();
    void Revive();
    void HandlePlayerDeath();

    // --- Revive State Management ---
    void SetBeingRevived(bool bState, AHama* InReviver = nullptr, float InReviveDuration = 0.0f, float InReviveEndTime = 0.0f);

    // --- Getters ---
    UFUNCTION(BlueprintPure, Category = "Health")
    bool IsDowned() const { return bIsDowned; }

    UFUNCTION(BlueprintPure, Category = "Health")
    bool IsBeingRevived() const { return bIsBeingRevived; }

    UFUNCTION(BlueprintPure, Category = "Health")
    float GetBleedOutPercent() const;

    UFUNCTION(BlueprintPure, Category = "Health")
    float GetActiveRevivePercent() const;

    AHama* GetCurrentReviver() const { return CurrentReviver.Get(); }

    // --- Replicated Properties ---
    UPROPERTY(ReplicatedUsing = OnRep_CurrentHealth)
    float CurrentHealth = 100.0f;

    UPROPERTY(Replicated)
    float MaxHealth = 100.0f;

    UPROPERTY(ReplicatedUsing = OnRep_IsDowned)
    bool bIsDowned = false;

    UPROPERTY(ReplicatedUsing = OnRep_IsBeingRevived)
    bool bIsBeingRevived = false;

    // BO3 Bleed-Out Sync Data (Zero-Tick Bandwidth)
    UPROPERTY(Replicated)
    float BleedOutEndTime = 0.0f;

    UPROPERTY(Replicated)
    float PausedBleedOutRemaining = 0.0f;

    // Synced so the Downed Player CAN ALSO see the Revive Progress Bar like BO3
    UPROPERTY(Replicated)
    float BeingRevivedTotalDuration = 0.0f;

    UPROPERTY(Replicated)
    float BeingRevivedEndTime = 0.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Health|Config")
    float DeathTime = 45.0f; // BO3 Standard 45s Bleed-out

    UPROPERTY(EditDefaultsOnly, Category = "Health|Config")
    float SoloReviveTime = 10.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Health|Config")
    float HealthTickGenerate = 0.2f; // 5Hz instead of spamming network

    UPROPERTY(EditDefaultsOnly, Category = "Health|Config")
    float HealthGenerateDelay = 4.0f;

protected:
    UFUNCTION()
    void OnRep_CurrentHealth(float OldHealth);

    UFUNCTION()
    void OnRep_IsDowned();

    UFUNCTION()
    void OnRep_IsBeingRevived();

    void RegenerateHealth();
    float GetServerTime() const;

private:
    UPROPERTY()
    TObjectPtr<AHama> OwnerCharacter = nullptr;

    UPROPERTY()
    TObjectPtr<UHamaComponent> OwnerComponent = nullptr;

    TWeakObjectPtr<AHama> CurrentReviver = nullptr;

    FTimerHandle RegenerateHealthTimer;
    FTimerHandle DownTimerHandle;
    FTimerHandle QuickReviveTimerHandle;
};