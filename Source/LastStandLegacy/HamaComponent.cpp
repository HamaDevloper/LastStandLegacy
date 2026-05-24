// Fill out your copyright notice in the Description page of Project Settings.

#include "HamaComponent.h"
#include "Net/UnrealNetwork.h"
#include "Hama.h"
#include "HamaMovementComponent.h"

// Sets default values for this component's properties
UHamaComponent::UHamaComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

// Called when the game starts
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

	DOREPLIFETIME_CONDITION(UHamaComponent, bIsSprinting, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(UHamaComponent, bIsAiming, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(UHamaComponent, bIsFiring, COND_SkipOwner);
}

void UHamaComponent::SetAiming(bool bNewAiming)
{
	if (bIsAiming == bNewAiming) return;
	bIsAiming = bNewAiming;

	// ✅ ڕاستەوخۆ فڵاگەکە دەدەین بە مۆڤمێنت کۆمپۆنێنت، ئەو خۆی نێتۆرکەکە لە ڕێگەی Saved Moves ڕێکدەخات
	if (MoveComp)
	{
		MoveComp->bAiming = bIsAiming;
	}
}

void UHamaComponent::SetFiring(bool bNewFiring)
{
	if (bIsFiring == bNewFiring) return;
	bIsFiring = bNewFiring;

	// ✅ ڕاستەوخۆ بۆ تەقەکردنیش بە هەمان شێوە
	if (MoveComp)
	{
		MoveComp->bFiring = bIsFiring;
	}
}

// 🌟 ئەم فەنکشنە تەنها لەسەر سێرڤەر جێبەجێ دەبێت
void UHamaComponent::IncreaseMaxStamina(float AmountToAdd)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) return;

	// ١. دۆزینەوەی ڕێژەی ئێستای ستەمینا پێش گۆڕینی ماکس (مثلاً 50 / 100 = 0.5)
	float CurrentStaminaPercentage = (MaxStamina > 0.f) ? (Stamina / MaxStamina) : 1.f;

	// ٢. بەرزکردنەوەی ماکس ستەمینا لای سێرڤەر
	MaxStamina += AmountToAdd;

	// ٣. حیسابکردنەوەی ستەمینای نوێ بە پێی هەمان ڕێژەی کۆن (مثلاً 0.5 * 200 = 100)
	Stamina = MaxStamina * CurrentStaminaPercentage;

	// ٤. ئەگەر غار نادات، دڵنیا دەبینەوە کە تایمەری ڕیجێنەکە کار دەکات بۆ ئەوەی دەستبەجێ پڕ ببێتەوە
	if (!bIsSprinting && !GetWorld()->GetTimerManager().IsTimerActive(StaminaRegenTimerHandle))
	{
		// ئەگەر لە ناو کاتی سزادا نەبووین، با ڕاستەوخۆ دەست بە ڕیجێن بکاتەوە
		if (!GetWorld()->GetTimerManager().IsTimerActive(StaminaPenaltyTimerHandle))
		{
			GetWorld()->GetTimerManager().SetTimer(StaminaRegenTimerHandle, this, &UHamaComponent::RegenerateStamina, 0.1f, true, NormalDelayStamina);
		}
	}

	// ٥. ناردنی فەرمان بۆ کڵایەنت بۆ ئەوەی حساباتی لۆکاڵی ڕێک بخاتەوە
	Client_OnMaxStaminaChanged(MaxStamina, Stamina);
}

// 🌟 ئەم فەنکشنە تەنها لای کڵایەنتی خاوەن (Local Client) لێدەدات
void UHamaComponent::Client_OnMaxStaminaChanged_Implementation(float NewMaxStamina, float NewCurrentStamina)
{
	// کڵایەنت بەهاکان یەکسان دەکاتەوە لەگەڵ سێرڤەر بۆ ئەوەی تووشی Desync نەبن
	MaxStamina = NewMaxStamina;
	Stamina = NewCurrentStamina;
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

void UHamaComponent::SetSprinting(bool bNewSprinting)
{
	if (bIsSprinting == bNewSprinting) return;

	GetWorld()->GetTimerManager().ClearTimer(StaminaRegenTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(StaminaDrainTimerHandle);

	bIsSprinting = bNewSprinting;

	if (MoveComp)
	{
		MoveComp->bSprinting = bIsSprinting;
	}

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

void UHamaComponent::DrainStamina()
{
	if (!OwnerCharacter || !MoveComp) return;

	if (Stamina <= 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(StaminaPenaltyTimerHandle, PenaltyStamina, false);
		SetSprinting(false);
		if (OwnerCharacter && OwnerCharacter->IsAimButtonHeld())
		{
			SetAiming(true);             // Aimەکەی بۆ چالاک بکەرەوە
			OwnerCharacter->OnAim(true); // زوومی کامێراکەی (TPP Zoom) بۆ بگەڕێنەوە
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
	if (Stamina >= MaxStamina)
	{
		GetWorld()->GetTimerManager().ClearTimer(StaminaRegenTimerHandle);
		return;
	}
	Stamina = FMath::Clamp(Stamina + StaminaRegenRate * 0.1f, 0.f, MaxStamina);
}

void UHamaComponent::OnRep_Sprinting()
{
	if (MoveComp)
	{
		MoveComp->bSprinting = bIsSprinting;
		MoveComp->MaxWalkSpeed = MoveComp->GetMaxSpeed();
	}
}

void UHamaComponent::OnRep_Aiming()
{
	if (MoveComp)
	{
		MoveComp->bAiming = bIsAiming;
		MoveComp->MaxWalkSpeed = MoveComp->GetMaxSpeed();
	}
}

void UHamaComponent::OnRep_Firing()
{
	if (MoveComp)
	{
		MoveComp->bFiring = bIsFiring;
		MoveComp->MaxWalkSpeed = MoveComp->GetMaxSpeed();
	}
}