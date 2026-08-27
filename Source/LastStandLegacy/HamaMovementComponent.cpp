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

    DefaultGroundFriction = 8.0f;
    DefaultBrakingDecelerationWalking = 2048.0f;
}

void UHamaMovementComponent::BeginPlay()
{
    Super::BeginPlay();

    DefaultGroundFriction = GroundFriction;
    DefaultBrakingDecelerationWalking = BrakingDecelerationWalking;

    if (CharacterOwner)
    {
        CachedHamaComp = CharacterOwner->FindComponentByClass<UHamaComponent>();
    }
}

UHamaComponent* UHamaMovementComponent::GetHamaComp()
{
    if (!CachedHamaComp.IsValid() && CharacterOwner)
    {
        CachedHamaComp = CharacterOwner->FindComponentByClass<UHamaComponent>();
    }
    return CachedHamaComp.Get();
}

float UHamaMovementComponent::GetMaxSpeed() const
{
    if (bDowned) return DownSpeed;
    if (bSlide) return SlideSpeed;
    if (bAiming) return AimSpeed;
    if (bSprinting) return SprintSpeed;

    return Super::GetMaxSpeed();
}

FVector UHamaMovementComponent::ScaleInputAcceleration(const FVector& InputAcceleration) const
{
    if (bSlide || bDiving)
    {
        return FVector::ZeroVector;
    }

    return Super::ScaleInputAcceleration(InputAcceleration);
}

void UHamaMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
    Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

    if (bAiming)
    {
        bSprinting = false;
    }

    if (bSlide && Velocity.SizeSquared2D() < FMath::Square(200.f))
    {
        bSlide = false;
    }

    if (bDiving && bWasDiving && MovementMode != MOVE_Falling)
    {
        bDiving = false;
    }

    if (bSlide && !bWasSliding)
    {
        if (CharacterOwner)
        {
            FVector SlideDirection = CharacterOwner->GetActorForwardVector();
            Velocity = FVector(SlideDirection.X * SlideSpeed, SlideDirection.Y * SlideSpeed, Velocity.Z);
        }
    }

    // 🟢 [FIX 1]: دڵنیابوونەوە لەسەر زەوی بوون بەر لە دایڤ (ڕێگریکردن لە Mid-Air Dive Exploit)
    if (bDiving && !bWasDiving && IsMovingOnGround())
    {
        if (CharacterOwner)
        {
            FVector DiveDirection = CharacterOwner->GetActorForwardVector();
            Velocity = FVector(DiveDirection.X * DiveImpulseHorizontal, DiveDirection.Y * DiveImpulseHorizontal, DiveImpulseVertical);

            SetMovementMode(MOVE_Falling);
            CurrentFloor.Clear();
        }
    }

    if (bSlide)
    {
        GroundFriction = 0.5f;
        BrakingDecelerationWalking = 200.f;
    }
    else
    {
        GroundFriction = DefaultGroundFriction;
        BrakingDecelerationWalking = DefaultBrakingDecelerationWalking;
    }

    if (CharacterOwner && CharacterOwner->HasAuthority())
    {
        if (bWasSliding != bSlide || bWasDiving != bDiving)
        {
            if (UHamaComponent* HamaComp = GetHamaComp())
            {
                HamaComp->SyncStatesFromCMC(bSlide, bDiving, bSprinting);
            }
        }
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
}

// ================= SAVED MOVE =================
void UHamaMovementComponent::FSavedMove_Hama::Clear()
{
    FSavedMove_Character::Clear();

    bSavedWantsToSprint = false;
    bSavedWantsToAim = false;
    bSavedWantsToDive = false;
    bSavedWantsToSlide = false;
    bSavedWasSliding = false;
    bSavedWasDiving = false;
}

void UHamaMovementComponent::FSavedMove_Hama::SetMoveFor(ACharacter* C, float DT, FVector const& Accel, FNetworkPredictionData_Client_Character& Data)
{
    FSavedMove_Character::SetMoveFor(C, DT, Accel, Data);

    UHamaMovementComponent* Comp = static_cast<UHamaMovementComponent*>(C->GetCharacterMovement());
    check(Comp);

    bSavedWantsToAim = Comp->bAiming;
    bSavedWantsToSprint = Comp->bSprinting;
    bSavedWantsToDive = Comp->bDiving;
    bSavedWantsToSlide = Comp->bSlide;
    bSavedWasSliding = Comp->bWasSliding;
    bSavedWasDiving = Comp->bWasDiving;
}

void UHamaMovementComponent::FSavedMove_Hama::PrepMoveFor(ACharacter* C)
{
    FSavedMove_Character::PrepMoveFor(C);

    UHamaMovementComponent* Comp = static_cast<UHamaMovementComponent*>(C->GetCharacterMovement());
    check(Comp);

    Comp->bSprinting = bSavedWantsToSprint;
    Comp->bAiming = bSavedWantsToAim;
    Comp->bDiving = bSavedWantsToDive;
    Comp->bSlide = bSavedWantsToSlide;
    Comp->bWasSliding = bSavedWasSliding;
    Comp->bWasDiving = bSavedWasDiving;
}

bool UHamaMovementComponent::FSavedMove_Hama::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* C, float MaxDelta) const
{
    const FSavedMove_Hama* Other = static_cast<const FSavedMove_Hama*>(NewMove.Get());

    if (bSavedWantsToSprint != Other->bSavedWantsToSprint) return false;
    if (bSavedWantsToAim != Other->bSavedWantsToAim) return false;
    if (bSavedWantsToDive != Other->bSavedWantsToDive) return false;
    if (bSavedWantsToSlide != Other->bSavedWantsToSlide) return false;
    if (bSavedWasSliding != Other->bSavedWasSliding) return false;
    if (bSavedWasDiving != Other->bSavedWasDiving) return false;

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