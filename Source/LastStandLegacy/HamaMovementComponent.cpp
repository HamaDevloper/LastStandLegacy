// Fill out your copyright notice in the Description page of Project Settings.

#include "HamaMovementComponent.h"
#include "GameFramework/Character.h"
#include "HamaComponent.h"

UHamaMovementComponent::UHamaMovementComponent()
{
    bSprinting = false;
    bAiming = false;
    bDowned = false;
    bSlide = false;
}

float UHamaMovementComponent::GetMaxSpeed() const
{
    float MaxSpeed = Super::GetMaxSpeed();

    if (bDowned) return DownSpeed;
    if (bSlide) return SlideSpeed;
    if (bAiming) return AimSpeed;
    if (bSprinting) return SprintSpeed;

    return MaxSpeed;
}

// ================= COMPRESSED FLAGS (SERVER READS THIS) =================
void UHamaMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);

    bSprinting = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
    bAiming = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
    bDowned = (Flags & FSavedMove_Character::FLAG_Custom_2) != 0;
    bSlide = (Flags & FSavedMove_Character::FLAG_Custom_3) != 0;

    if (CharacterOwner && CharacterOwner->HasAuthority())
    {
        if (UHamaComponent* HamaComp = CharacterOwner->FindComponentByClass<UHamaComponent>())
        {
            if (bSprinting) HamaComp->StartSprinting();
            else            HamaComp->StopSprinting();

            HamaComp->SetAiming(bAiming);
            HamaComp->SetDown(bDowned);

            if (bSlide) HamaComp->StartSlide();
            else        HamaComp->StopSlide();
        }
    }
}

// ================= SAVED MOVE =================
void UHamaMovementComponent::FSavedMove_Hama::Clear()
{
    Super::Clear();
    bSavedWantsToSprint = false;
    bSavedWantsToAim = false;
    bSavedIsDowned = false;
    bSavedWantsToSlide = false;
}

void UHamaMovementComponent::FSavedMove_Hama::SetMoveFor(ACharacter* C, float DT, FVector const& Accel, FNetworkPredictionData_Client_Character& Data)
{
    Super::SetMoveFor(C, DT, Accel, Data);
    if (UHamaMovementComponent* Comp = Cast<UHamaMovementComponent>(C->GetCharacterMovement()))
    {
        bSavedWantsToAim = Comp->bAiming;
        bSavedWantsToSprint = Comp->bSprinting;
        bSavedIsDowned = Comp->bDowned;
        bSavedWantsToSlide = Comp->bSlide;
    }
}

void UHamaMovementComponent::FSavedMove_Hama::PrepMoveFor(ACharacter* C)
{
    Super::PrepMoveFor(C);
    if (UHamaMovementComponent* Comp = Cast<UHamaMovementComponent>(C->GetCharacterMovement()))
    {
        Comp->bSprinting = bSavedWantsToSprint;
        Comp->bAiming = bSavedWantsToAim;
        Comp->bDowned = bSavedIsDowned;
        Comp->bSlide = bSavedWantsToSlide;
    }
}

bool UHamaMovementComponent::FSavedMove_Hama::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* C, float MaxDelta) const
{
    const FSavedMove_Hama* Other = static_cast<const FSavedMove_Hama*>(NewMove.Get());

    if (bSavedWantsToSprint != Other->bSavedWantsToSprint) return false;
    if (bSavedWantsToAim != Other->bSavedWantsToAim) return false;
    if (bSavedIsDowned != Other->bSavedIsDowned) return false;
    if (bSavedWantsToSlide != Other->bSavedWantsToSlide) return false; // 🚀 ڕێگری لە تێکەڵبوون

    return Super::CanCombineWith(NewMove, C, MaxDelta);
}

uint8 UHamaMovementComponent::FSavedMove_Hama::GetCompressedFlags() const
{
    uint8 Result = Super::GetCompressedFlags();

    if (bSavedWantsToSprint) Result |= FSavedMove_Character::FLAG_Custom_0;
    if (bSavedWantsToAim)    Result |= FSavedMove_Character::FLAG_Custom_1;
    if (bSavedIsDowned)      Result |= FSavedMove_Character::FLAG_Custom_2;
    if (bSavedWantsToSlide)  Result |= FSavedMove_Character::FLAG_Custom_3; // 🚀 فڵاگی کۆتایی

    return Result;
}

// ================= PREDICTION DATA =================
UHamaMovementComponent::FNetworkPredictionData_Client_Hama::FNetworkPredictionData_Client_Hama(const UCharacterMovementComponent& MoveComponent)
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