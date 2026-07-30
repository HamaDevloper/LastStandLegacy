// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HamaComponent.generated.h"

class AHama;
class UHamaMovementComponent;
class ALastStandLegacyGameState;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LASTSTANDLEGACY_API UHamaComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHamaComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;

    /* ================================================================================================
     *                                    MOVEMENT & ACTION INPUTS
     * ================================================================================================ */
public:
    void StartSprinting();
    void StopSprinting();

    void StartSlide();
    void StopSlide();

    void StartDive();
    void StopDive();

    void SetAiming(bool bNewAiming);
    void SetDowned(bool NewValue);

    /** Helpers for state conditions */
    bool CanSprint() const;
    bool CanDive() const;

    /* ================================================================================================
     *                                    STAMINA & STATE GETTERS
     * ================================================================================================ */
public:
    UFUNCTION(BlueprintPure, Category = "Hama|Stamina")
    float GetStamina() const { return Stamina; }

    UFUNCTION(BlueprintPure, Category = "Hama|Stamina")
    float GetMaxStamina() const { return MaxStamina; }

    UFUNCTION(BlueprintPure, Category = "Hama|State")
    bool IsSprinting() const { return bIsSprinting; }

    UFUNCTION(BlueprintPure, Category = "Hama|State")
    bool IsAiming() const { return bIsAiming; }

    UFUNCTION(BlueprintPure, Category = "Hama|State")
    bool IsSlide() const { return bIsSlide; }

    UFUNCTION(BlueprintPure, Category = "Hama|State")
    bool IsDowned() const { return bIsDowned; }

    UFUNCTION(BlueprintPure, Category = "Hama|State")
    bool IsDiving() const { return bIsDiving; }

    void DecreaseStamina(float Amount);
    void UpgradeMaxStamina(float NewMaxStamina);
    void ResetStamina();

    /* ================================================================================================
     *                                 PROTECTED PROPERTIES & REPLICATION
     * ================================================================================================ */
protected:
    // --- STAMINA SETTINGS ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hama|Stamina")
    float Stamina = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hama|Stamina")
    float MaxStamina = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hama|Stamina")
    float StaminaRegenRate = 15.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hama|Stamina")
    float StaminaDrainRate = 20.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hama|Stamina")
    float PenaltyStamina = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hama|Stamina")
    float NormalDelayStamina = 0.5f;

    // --- REPLICATED STATES ---
    UPROPERTY(ReplicatedUsing = OnRep_Sprinting, EditAnywhere, BlueprintReadOnly, Category = "Hama|State")
    bool bIsSprinting = false;

    UPROPERTY(ReplicatedUsing = OnRep_Aiming, EditAnywhere, BlueprintReadOnly, Category = "Hama|State")
    bool bIsAiming = false;

    UPROPERTY(ReplicatedUsing = OnRep_Slide, EditAnywhere, BlueprintReadOnly, Category = "Hama|State")
    bool bIsSlide = false;

    UPROPERTY(ReplicatedUsing = OnRep_Down, EditAnywhere, BlueprintReadOnly, Category = "Hama|State")
    bool bIsDowned = false;

    UPROPERTY(ReplicatedUsing = OnRep_Dive, EditAnywhere, BlueprintReadOnly, Category = "Hama|State")
    bool bIsDiving = false;

    // --- INTERNAL LOGIC ---
    void SetSprinting(bool bNewSprinting);
    void DrainStamina();
    void RegenerateStamina();

    // --- ONREP NOTIFIES ---
    UFUNCTION()
    void OnRep_Sprinting();

    UFUNCTION()
    void OnRep_Aiming();

    UFUNCTION()
    void OnRep_Slide();

    UFUNCTION()
    void OnRep_Down();

    UFUNCTION()
    void OnRep_Dive();

    /* ================================================================================================
     *                                 PRIVATE CACHED POINTERS & TIMERS
     * ================================================================================================ */
private:
    FTimerHandle StaminaDrainTimerHandle;
    FTimerHandle StaminaRegenTimerHandle;
    FTimerHandle StaminaPenaltyTimerHandle;

    UPROPERTY()
    TObjectPtr<AHama> OwnerCharacter;

    UPROPERTY()
    TObjectPtr<UHamaMovementComponent> MoveComp;

    UPROPERTY()
    TObjectPtr<ALastStandLegacyGameState> GSCache;
};