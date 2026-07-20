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
    // Sets default values for this component's properties
    UHamaComponent();
protected:
    // Called when the game starts
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Hama|Stamina")
    float Stamina;
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

public:
    void StartSprinting();
    void StopSprinting();
    void StartSlide();
    void StopSlide();
    void StartDive();
    void StopDive();


    UFUNCTION(Server, Reliable)
    void Server_SetSlideState(bool bNewSlideState);
    UFUNCTION(Server, Reliable)
    void Server_SetDiving(bool bNewDiveState);
    // FIX: if the server rejects a Dive request (not enough stamina), this RPC tells the
    // owning client to roll back its predicted bIsDiving state to avoid desync.
    UFUNCTION(Client, Reliable)
    void Client_RejectDive();
    float GetStamina() const { return Stamina; }
protected:
    void SetSprinting(bool bNewSprinting);
    void DrainStamina();
    void RegenerateStamina();

    UFUNCTION(Server, Reliable)
    void Server_SetSprint(bool bNewSprinting);

public:
    void UpgradeMaxStamina(float NewMaxStamina);
    void ResetStamina();
    void SetAiming(bool bNewAiming);

    UFUNCTION(Server, Reliable)
    void Server_SetAiming(bool bNewAiming);

    UPROPERTY(ReplicatedUsing = OnRep_Sprinting, EditAnywhere, BlueprintReadWrite, Category = "Hama|Stamina")
    bool bIsSprinting = false;
    UPROPERTY(ReplicatedUsing = OnRep_Aiming, EditAnywhere, BlueprintReadWrite, Category = "Hama|Stamina")
    bool bIsAiming = false;
    UPROPERTY(ReplicatedUsing = OnRep_Slide, EditAnywhere, BlueprintReadWrite, Category = "Hama|Stamina")
    bool bIsSlide = false;
    UPROPERTY(ReplicatedUsing = OnRep_Down, BlueprintReadWrite, Category = "Hama|State", meta = (AllowPrivateAccess = "true"))
    bool bIsDowned = false;
    UPROPERTY(ReplicatedUsing = OnRep_Dive, BlueprintReadOnly, Category = "Hama|State", meta = (AllowPrivateAccess = "true"))
    bool bIsDiving = false;

    bool IsSprinting() const { return bIsSprinting; }
    bool IsSlide() const { return bIsSlide; }
    bool IsAiming() const { return bIsAiming; }
    bool IsDowned() const { return bIsDowned; }
    bool IsDiving() const { return bIsDiving; }
    void SetDowned(bool NewValue);

    UFUNCTION(BlueprintPure, Category = "Hama|Stamina")
    float GetCurrentStamina() const { return Stamina; }
    void DecreaseStamina(float Amount);
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
};