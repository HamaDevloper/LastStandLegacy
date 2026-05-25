// Fill out your copyright notice in the Description page of Project Settings.

#include "HamaComponent.h"
#include "Net/UnrealNetwork.h"
#include "Hama.h"
#include "HamaMovementComponent.h"

UHamaComponent::UHamaComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UHamaComponent::BeginPlay()
{
	Super::BeginPlay();

	Stamina = MaxStamina;
	OwnerCharacter = Cast<AHama>(GetOwner());

	if (OwnerCharacter)
	{
		MoveComp = OwnerCharacter->FindComponentByClass<UHamaMovementComponent>();
	}
}

void UHamaComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// کڵایەنتی خۆی ئەمەی پێویست نییە چونکە بە SavedMoves پێشبینی دەکرێت، تەنها بۆ خەڵکی ترە
	DOREPLIFETIME_CONDITION(UHamaComponent, bIsSprinting, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(UHamaComponent, bIsAiming, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(UHamaComponent, bIsFiring, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(UHamaComponent, bIsSlide, COND_SkipOwner);
}

void UHamaComponent::SetAiming(bool bNewAiming)
{
	if (bIsAiming == bNewAiming) return;
	bIsAiming = bNewAiming;

	if (MoveComp) MoveComp->bAiming = bIsAiming;
}

void UHamaComponent::SetFiring(bool bNewFiring)
{
	if (bIsFiring == bNewFiring) return;
	bIsFiring = bNewFiring;

	if (MoveComp) MoveComp->bFiring = bIsFiring;
}

void UHamaComponent::StartSprinting()
{
	if (bIsSprinting) return;
	SetSprinting(true);
}

void UHamaComponent::StopSprinting()
{
	if (!bIsSprinting) return;
	SetSprinting(false);
}

void UHamaComponent::StartSlide()
{
	if (bIsSlide) return;

	bIsSlide = true;
	if (!OwnerCharacter->HasAuthority())
	{
		Server_SetSlideState(true);
	}
}

void UHamaComponent::StopSlide()
{
	if (!bIsSlide) return;

	bIsSlide = false;
	if (!OwnerCharacter->HasAuthority())
	{
		Server_SetSlideState(false);
	}
}

void UHamaComponent::Server_SetSlideState_Implementation(bool bNewSlideState)
{
	if (bIsSlide == bNewSlideState) return;
	bIsSlide = bNewSlideState;

	if (GetNetMode() == NM_ListenServer)
	{
		OnRep_Slide();
	}
}

void UHamaComponent::SetSprinting(bool bNewSprinting)
{
	if (bIsSprinting == bNewSprinting) return;

	GetWorld()->GetTimerManager().ClearTimer(StaminaRegenTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(StaminaDrainTimerHandle);

	bIsSprinting = bNewSprinting;

	if (MoveComp) MoveComp->bSprinting = bIsSprinting;

	if (bIsSprinting)
	{
		GetWorld()->GetTimerManager().ClearTimer(StaminaPenaltyTimerHandle);
		GetWorld()->GetTimerManager().SetTimer(StaminaDrainTimerHandle, this, &UHamaComponent::DrainStamina, 0.1f, true);
	}
	else
	{
		if (GetWorld()->GetTimerManager().IsTimerActive(StaminaPenaltyTimerHandle)) return;
		GetWorld()->GetTimerManager().SetTimer(StaminaRegenTimerHandle, this, &UHamaComponent::RegenerateStamina, 0.1f, true, NormalDelayStamina);
	}
}

// ============== STAMINA LOGIC ==============

void UHamaComponent::DrainStamina()
{
	if (!OwnerCharacter || !MoveComp) return;

	if (Stamina <= 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(StaminaPenaltyTimerHandle, PenaltyStamina, false);
		SetSprinting(false);
		if (OwnerCharacter && OwnerCharacter->IsAimButtonHeld())
		{
			SetAiming(true);
			OwnerCharacter->OnAim(true);
		}
		GetWorld()->GetTimerManager().SetTimer(StaminaRegenTimerHandle, this, &UHamaComponent::RegenerateStamina, 0.1f, true, PenaltyStamina);
		return;
	}

	if (MoveComp->IsFalling()) return;

	FVector InputVector = OwnerCharacter->GetLastMovementInputVector();
	if (InputVector.IsNearlyZero())
	{
		SetSprinting(false);
		return;
	}

	FVector CurrentVelocity = OwnerCharacter->GetVelocity();
	CurrentVelocity.Z = 0.f;

	if (CurrentVelocity.SizeSquared() < 10000.f) return;

	FVector ForwardVector = OwnerCharacter->GetActorForwardVector();
	InputVector.Normalize();
	float ForwardMotion = FVector::DotProduct(InputVector, ForwardVector);

	if (ForwardMotion < 0.5f)
	{
		SetSprinting(false);
		return;
	}
	Stamina = FMath::Clamp(Stamina - StaminaDrainRate * 0.1f, 0.f, MaxStamina);
}

void UHamaComponent::RegenerateStamina()
{
	if (bIsSlide) return;
	if (Stamina >= MaxStamina)
	{
		GetWorld()->GetTimerManager().ClearTimer(StaminaRegenTimerHandle);
		return;
	}
	Stamina = FMath::Clamp(Stamina + StaminaRegenRate * 0.1f, 0.f, MaxStamina);
}

void UHamaComponent::IncreaseMaxStamina(float AmountToAdd)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) return;

	float CurrentStaminaPercentage = (MaxStamina > 0.f) ? (Stamina / MaxStamina) : 1.f;
	MaxStamina += AmountToAdd;
	Stamina = MaxStamina * CurrentStaminaPercentage;

	if (!bIsSprinting && !GetWorld()->GetTimerManager().IsTimerActive(StaminaRegenTimerHandle) && !GetWorld()->GetTimerManager().IsTimerActive(StaminaPenaltyTimerHandle))
	{
		GetWorld()->GetTimerManager().SetTimer(StaminaRegenTimerHandle, this, &UHamaComponent::RegenerateStamina, 0.1f, true, NormalDelayStamina);
	}

	Client_OnMaxStaminaChanged(MaxStamina, Stamina);
}

void UHamaComponent::ResetMaxStamina()
{
	// دڵنیابوونەوە لەوەی تەنها سێرڤەر ئەم کارە دەکات
	if (GetOwner() && !GetOwner()->HasAuthority()) return;

	// گەڕاندنەوەی ماکس ستامینا بۆ ١٠٠ (یان هەر بڕێک کە دیفۆڵتە لای خۆت)
	MaxStamina = 100.f;

	// بەکارهێنانەوەی هەمان ئەو فەنکشنەی خۆت بۆ ئاگادارکردنەوەی کڵایەنتەکە
	Client_OnMaxStaminaChanged(MaxStamina, Stamina);
}

void UHamaComponent::Client_OnMaxStaminaChanged_Implementation(float NewMaxStamina, float NewCurrentStamina)
{
	MaxStamina = NewMaxStamina;
	Stamina = NewCurrentStamina;
}

// ================= ON_REP FUNCTIONS (SIMULATED PROXIES) =================

void UHamaComponent::OnRep_Sprinting()
{
	if (OwnerCharacter && OwnerCharacter->IsLocallyControlled()) return;
	if (MoveComp)
	{
		MoveComp->bSprinting = bIsSprinting;
	}
}

void UHamaComponent::OnRep_Aiming()
{
	if (OwnerCharacter && OwnerCharacter->IsLocallyControlled()) return;
	if (MoveComp)
	{
		MoveComp->bAiming = bIsAiming;
	}
}

void UHamaComponent::OnRep_Firing()
{
	if (OwnerCharacter && OwnerCharacter->IsLocallyControlled()) return;
	if (MoveComp)
	{
		MoveComp->bFiring = bIsFiring;
	}
}

void UHamaComponent::OnRep_Slide()
{
	if (OwnerCharacter && OwnerCharacter->IsLocallyControlled()) return;

	UAnimInstance* AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance();
	if (!AnimInstance || !OwnerCharacter->SlideMontage) return;

	if (bIsSlide)
	{
		AnimInstance->Montage_Play(OwnerCharacter->SlideMontage);
	}
	else
	{
		AnimInstance->Montage_Stop(0.2f, OwnerCharacter->SlideMontage);
	}
}
