// Fill out your copyright notice in the Description page of Project Settings.

#include "HamaMovementComponent.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"

// ================= CONSTRUCTOR =================

UHamaMovementComponent::UHamaMovementComponent()
{
	bSprinting = false;
	bAiming = false;
	bFiring = false;
}

float UHamaMovementComponent::GetMaxSpeed() const
{
	float MaxSpeed = Super::GetMaxSpeed();

	if (bAiming)
	{
		return AimSpeed;
	}

	if (IsCrouching() || bFiring)
	{
		return 300.f;
	}

	if (bSprinting)
	{
		return SprintSpeed;
	}

	return MaxSpeed;
}

// ================= COMPRESSED FLAGS =================

void UHamaMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	// سێرڤەر لێرەدا فڵاگەکان دەخوێنێتەوە کە کڵایەنت بۆی ناردووە
	bSprinting = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
	bAiming = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
	bFiring = (Flags & FSavedMove_Character::FLAG_Custom_2) != 0;
}

// ================= SAVED MOVE =================

void UHamaMovementComponent::FSavedMove_Hama::Clear()
{
	Super::Clear();
	bSavedWantsToSprint = false;
	bSavedWantsToAim = false;
	bSavedWantsToFire = false;
}

void UHamaMovementComponent::FSavedMove_Hama::SetMoveFor(
	ACharacter* C,
	float DT,
	FVector const& Accel,
	FNetworkPredictionData_Client_Character& Data)
{
	Super::SetMoveFor(C, DT, Accel, Data);
	if (UHamaMovementComponent* Comp = Cast<UHamaMovementComponent>(C->GetCharacterMovement()))
	{
		bSavedWantsToAim = Comp->bAiming;
		bSavedWantsToFire = Comp->bFiring;
		bSavedWantsToSprint = Comp->bSprinting;
	}
}

void UHamaMovementComponent::FSavedMove_Hama::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);
	if (UHamaMovementComponent* Comp = Cast<UHamaMovementComponent>(C->GetCharacterMovement()))
	{
		Comp->bSprinting = bSavedWantsToSprint;
		Comp->bAiming = bSavedWantsToAim;
		Comp->bFiring = bSavedWantsToFire;
	}
}

bool UHamaMovementComponent::FSavedMove_Hama::CanCombineWith(
	const FSavedMovePtr& NewMove,
	ACharacter* C,
	float MaxDelta) const
{
	const FSavedMove_Hama* Other = static_cast<const FSavedMove_Hama*>(NewMove.Get());

	if (bSavedWantsToSprint != Other->bSavedWantsToSprint) return false;
	if (bSavedWantsToAim != Other->bSavedWantsToAim) return false;
	if (bSavedWantsToFire != Other->bSavedWantsToFire) return false;

	return Super::CanCombineWith(NewMove, C, MaxDelta);
}

uint8 UHamaMovementComponent::FSavedMove_Hama::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();

	// ✅ چاکسازی ڕێکخستنی بیتەکان (Bitwise) بە پێی ڕێزبەندی فەرمی لۆجیکەکە بۆ ئەوەی داتاکان تێکەڵ نەبن
	if (bSavedWantsToSprint)
	{
		Result |= FSavedMove_Character::FLAG_Custom_0;
	}
	if (bSavedWantsToAim)
	{
		Result |= FSavedMove_Character::FLAG_Custom_1;
	}
	if (bSavedWantsToFire)
	{
		Result |= FSavedMove_Character::FLAG_Custom_2;
	}

	return Result;
}

// ================= PREDICTION DATA =================

UHamaMovementComponent::FNetworkPredictionData_Client_Hama::FNetworkPredictionData_Client_Hama(
	const UCharacterMovementComponent& MoveComponent)
	: Super(MoveComponent)
{
}

FSavedMovePtr UHamaMovementComponent::FNetworkPredictionData_Client_Hama::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_Hama());
}

FNetworkPredictionData_Client* UHamaMovementComponent::GetPredictionData_Client() const
{
	if (!ClientPredictionData)
	{
		UHamaMovementComponent* Mutable = const_cast<UHamaMovementComponent*>(this);
		Mutable->ClientPredictionData = new FNetworkPredictionData_Client_Hama(*this);
	}
	return ClientPredictionData;
}