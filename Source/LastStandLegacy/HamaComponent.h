// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HamaComponent.generated.h"

class AHama;
class UHamaMovementComponent;

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hama|Stamina")
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

	float GetStamina() const { return Stamina; }

protected:
	void SetSprinting(bool bNewSprinting);

	void DrainStamina();
	void RegenerateStamina();
	
	UFUNCTION(Client, Reliable)
	void Client_OnMaxStaminaChanged(float NewMaxStamina, float NewCurrentStamina);

public:
	void SetAiming(bool bNewAiming);
	void SetFiring(bool bNewFiring);

	void IncreaseMaxStamina(float AmountToAdd);

public:
	// ✅ ڤاریابڵەکان وەک خۆیان پارێزراون بەڵام ReplicatedUsing مان لابردووە چونکە Saved Moves خۆی کارەکە دەکات
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Hama|Stamina")
	bool bIsSprinting = false;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Hama|Stamina")
	bool bIsAiming = false;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Hama|Stamina")
	bool bIsFiring = false;

	bool IsSprinting() const { return bIsSprinting; }

private:
	FTimerHandle StaminaDrainTimerHandle;
	FTimerHandle StaminaRegenTimerHandle;
	FTimerHandle StaminaPenaltyTimerHandle;
	AHama* OwnerCharacter;
	UHamaMovementComponent* MoveComp;


	UFUNCTION()
	void OnRep_Sprinting();

	UFUNCTION()
	void OnRep_Aiming();

	UFUNCTION()
	void OnRep_Firing();

};