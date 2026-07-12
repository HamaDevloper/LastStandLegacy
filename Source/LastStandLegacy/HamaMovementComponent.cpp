#include "HamaMovementComponent.h"
#include "GameFramework/Character.h"
#include "HamaComponent.h"

UHamaMovementComponent::UHamaMovementComponent()
{
    bSprinting = false;
    bAiming = false;
    bDiving = false;
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

void UHamaMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
    Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

    // ١. پێدانی هێزی خلیسکان لە یەکەم فڕەیمی دەستپێکردندا بە شێوەیەکی سەلامەت بۆ تۆڕ
    if (bSlide && !bWasSliding)
    {
        if (CharacterOwner)
        {
            FVector SlideDirection = CharacterOwner->GetActorForwardVector();
            Velocity += SlideDirection * 800.f;
        }
    }

    // ٢. جێبەجێکردنی هێزی Dive
    if (bDiving && !bWasDiving)
    {
        if (CharacterOwner)
        {
            FVector DiveDirection = CharacterOwner->GetActorForwardVector() * 900.f;
            DiveDirection.Z = 350.f;
            Velocity += DiveDirection;

            // خستنە باری هەوا بۆ ئەوەی لێکخشانی زەوی نەیگرێت
            SetMovementMode(MOVE_Falling);
        }
    }

    // ٣. ڕێکخستنی لێکخشان لە کاتی خلیسکاندا
    if (bSlide)
    {
        GroundFriction = 0.5f;
        BrakingDecelerationWalking = 200.f;
    }
    else
    {
        GroundFriction = 8.0f;
        BrakingDecelerationWalking = 2048.f;
    }

    bWasSliding = bSlide;
    bWasDiving = bDiving;
}

void UHamaMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);

    bSprinting = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
    bAiming = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
    bDiving = (Flags & FSavedMove_Character::FLAG_Custom_2) != 0;
    bSlide = (Flags & FSavedMove_Character::FLAG_Custom_3) != 0;

    if (CharacterOwner && CharacterOwner->HasAuthority())
    {
        if (UHamaComponent* HamaComp = CharacterOwner->FindComponentByClass<UHamaComponent>())
        {
            HamaComp->bIsSprinting = bSprinting;
            HamaComp->bIsAiming = bAiming;
            HamaComp->bIsDiving = bDiving;
            HamaComp->bIsSlide = bSlide;
        }
    }
}

// ================= SAVED MOVE =================
void UHamaMovementComponent::FSavedMove_Hama::Clear()
{
    FSavedMove_Character::Clear();
    bSavedWantsToSprint = false;
    bSavedWantsToAim = false;
    bSavedWantsToDive = false;
    bSavedWantsToSlide = false;
}

void UHamaMovementComponent::FSavedMove_Hama::SetMoveFor(ACharacter* C, float DT, FVector const& Accel, FNetworkPredictionData_Client_Character& Data)
{
    FSavedMove_Character::SetMoveFor(C, DT, Accel, Data);
    if (UHamaMovementComponent* Comp = Cast<UHamaMovementComponent>(C->GetCharacterMovement()))
    {
        bSavedWantsToAim = Comp->bAiming;
        bSavedWantsToSprint = Comp->bSprinting;
        bSavedWantsToDive = Comp->bDiving;
        bSavedWantsToSlide = Comp->bSlide;
    }
}

void UHamaMovementComponent::FSavedMove_Hama::PrepMoveFor(ACharacter* C)
{
    FSavedMove_Character::PrepMoveFor(C);
    if (UHamaMovementComponent* Comp = Cast<UHamaMovementComponent>(C->GetCharacterMovement()))
    {
        Comp->bSprinting = bSavedWantsToSprint;
        Comp->bAiming = bSavedWantsToAim;
        Comp->bDiving = bSavedWantsToDive;
        Comp->bSlide = bSavedWantsToSlide;
    }
}

bool UHamaMovementComponent::FSavedMove_Hama::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* C, float MaxDelta) const
{
    const FSavedMove_Hama* Other = static_cast<const FSavedMove_Hama*>(NewMove.Get());

    if (bSavedWantsToSprint != Other->bSavedWantsToSprint) return false;
    if (bSavedWantsToAim != Other->bSavedWantsToAim) return false;
    if (bSavedWantsToDive != Other->bSavedWantsToDive) return false;
    if (bSavedWantsToSlide != Other->bSavedWantsToSlide) return false;

    return FSavedMove_Character::CanCombineWith(NewMove, C, MaxDelta);
}

uint8 UHamaMovementComponent::FSavedMove_Hama::GetCompressedFlags() const
{
    uint8 Result = FSavedMove_Character::GetCompressedFlags();

    if (bSavedWantsToSprint) Result |= FSavedMove_Character::FLAG_Custom_0;
    if (bSavedWantsToAim)    Result |= FSavedMove_Character::FLAG_Custom_1;
    if (bSavedWantsToDive)   Result |= FSavedMove_Character::FLAG_Custom_2;
    if (bSavedWantsToSlide)  Result |= FSavedMove_Character::FLAG_Custom_3;

    return Result;
}

// ================= PREDICTION DATA =================
UHamaMovementComponent::FNetworkPredictionData_Client_Hama::FNetworkPredictionData_Client_Hama(const UCharacterMovementComponent& MoveComponent)
    : FNetworkPredictionData_Client_Character(MoveComponent)
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