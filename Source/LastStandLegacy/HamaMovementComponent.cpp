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
    bWasSliding = false;
    bWasDiving = false;
    bPrevSlide = false;
    bPrevDiving = false;
    bPrevSprinting = false;

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
    if (!CachedHamaComp && CharacterOwner)
    {
        CachedHamaComp = CharacterOwner->FindComponentByClass<UHamaComponent>();
    }
    return CachedHamaComp;
}

float UHamaMovementComponent::GetMaxSpeed() const
{
    if (bDowned) return DownSpeed;
    if (bSlide) return SlideSpeed;

    if (bAiming)
    {
        return IsCrouching() ? AimCrouchSpeed : AimSpeed;
    }

    if (bSprinting) return SprintSpeed;
    if (IsCrouching()) return MaxWalkSpeedCrouched;

    return Super::GetMaxSpeed();
}

float UHamaMovementComponent::GetMaxAcceleration() const
{
    if (bSlide)
    {
        return 0.0f;
    }

    return Super::GetMaxAcceleration();
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

    if (CharacterOwner && CharacterOwner->IsLocallyControlled() && !CharacterOwner->bClientUpdating)
    {
        if (UHamaComponent* HamaComp = GetHamaComp())
        {
            bSprinting = HamaComp->bIsSprinting;
            bAiming = HamaComp->bIsAiming;
        }
    }

    const float SlideStopThresholdSquared = FMath::Square(SlideStopSpeed);
    if (bSlide && bWasSliding && (Velocity.SizeSquared2D() < SlideStopThresholdSquared || !IsMovingOnGround()))
    {
        bSlide = false;
    }

    if (bDiving && bWasDiving && MovementMode != MOVE_Falling)
    {
        bDiving = false;
    }

    if (bSprinting)
    {
        if (bDowned || bAiming || bSlide || bDiving || IsFalling())
        {
            bSprinting = false;
        }
    }

    if (bSlide && !bWasSliding)
    {
        if (IsMovingOnGround())
        {
            FVector SlideDir = CharacterOwner ? CharacterOwner->GetActorForwardVector().GetSafeNormal2D() : FVector::ForwardVector;

            if (SlideDir.IsNearlyZero())
            {
                SlideDir = Velocity.GetSafeNormal2D();
            }

            Velocity = FVector(SlideDir.X * SlideSpeed, SlideDir.Y * SlideSpeed, Velocity.Z);
        }
        else
        {
            bSlide = false;
        }
    }

    if (bDiving && !bWasDiving && IsMovingOnGround())
    {
        FVector DiveDir = CharacterOwner ? CharacterOwner->GetActorForwardVector().GetSafeNormal2D() : FVector::ForwardVector;

        if (DiveDir.IsNearlyZero())
        {
            DiveDir = Velocity.GetSafeNormal2D();
        }

        Velocity = FVector(DiveDir.X * DiveImpulseHorizontal, DiveDir.Y * DiveImpulseHorizontal, DiveImpulseVertical);
        SetMovementMode(MOVE_Falling);
        CurrentFloor.Clear();
    }

    if (bSlide && IsMovingOnGround())
    {
        GroundFriction = SlideGroundFriction;
        BrakingDecelerationWalking = SlideBrakingDeceleration;
    }
    else
    {
        GroundFriction = DefaultGroundFriction;
        BrakingDecelerationWalking = DefaultBrakingDecelerationWalking;
    }

    bWasSliding = bSlide;
    bWasDiving = bDiving;
}

void UHamaMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
    Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);

/*#if !UE_BUILD_SHIPPING
    if (GEngine && CharacterOwner && !CharacterOwner->bClientUpdating)
    {
        const bool bIsServer = CharacterOwner->HasAuthority();
        const bool bIsLocallyControlled = CharacterOwner->IsLocallyControlled();
        const FString RoleStr = bIsServer ? TEXT("[SERVER]") : (bIsLocallyControlled ? TEXT("[LOCAL CLIENT]") : TEXT("[REMOTE CLIENT]"));

        const uint64 BaseKey = bIsServer ? 100 : (bIsLocallyControlled ? 200 : 300);
        const FColor DisplayColor = bIsServer ? FColor::Cyan : (bIsLocallyControlled ? FColor::Green : FColor::Yellow);

        GEngine->AddOnScreenDebugMessage(
            BaseKey + 0, 0.0f, DisplayColor,
            FString::Printf(TEXT("%s | Vel2D: %.1f | Mode: %d"), *RoleStr, Velocity.Size2D(), (int32)MovementMode.GetValue())
        );

        GEngine->AddOnScreenDebugMessage(
            BaseKey + 1, 0.0f, DisplayColor,
            FString::Printf(TEXT("%s [CMC] -> Sprint: %d | Slide: %d | Dive: %d | Aim: %d"),
                *RoleStr, bSprinting ? 1 : 0, bSlide ? 1 : 0, bDiving ? 1 : 0, bAiming ? 1 : 0)
        );

        if (UHamaComponent* HamaComp = GetHamaComp())
        {
            const bool bMismatch = (bSlide != HamaComp->bIsSlide) || (bDiving != HamaComp->bIsDiving);
            GEngine->AddOnScreenDebugMessage(
                BaseKey + 2, 0.0f, bMismatch ? FColor::Orange : DisplayColor,
                FString::Printf(TEXT("%s [Comp] -> Sprint: %d | Slide: %d | Dive: %d | Aim: %d"),
                    *RoleStr, HamaComp->bIsSprinting ? 1 : 0, HamaComp->bIsSlide ? 1 : 0, HamaComp->bIsDiving ? 1 : 0, HamaComp->bIsAiming ? 1 : 0)
            );
        }
    }
#endif*/

    if (CharacterOwner && CharacterOwner->bClientUpdating)
    {
        return;
    }

    const bool bStateChanged = (bSlide != bPrevSlide) ||
        (bDiving != bPrevDiving) ||
        (bSprinting != bPrevSprinting);

    if (!bStateChanged)
    {
        return;
    }

    bPrevSlide = bSlide;
    bPrevDiving = bDiving;
    bPrevSprinting = bSprinting;

    UHamaComponent* HamaComp = GetHamaComp();
    if (!HamaComp) return;

    if (CharacterOwner && CharacterOwner->HasAuthority())
    {
        HamaComp->SyncStatesFromCMC(bSlide, bDiving, bSprinting);
    }
    else if (CharacterOwner && CharacterOwner->IsLocallyControlled())
    {
        if (!bSlide && HamaComp->bIsSlide)
        {
            HamaComp->StopSlide();
        }

        if (!bSprinting && HamaComp->bIsSprinting)
        {
            HamaComp->SetSprinting(false);
        }
    }
}

void UHamaMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);

    bSprinting = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
    bAiming = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
    bDiving = (Flags & FSavedMove_Character::FLAG_Custom_2) != 0;
    bSlide = (Flags & FSavedMove_Character::FLAG_Custom_3) != 0;
}

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

void UHamaMovementComponent::FSavedMove_Hama::PostUpdate(ACharacter* C, EPostUpdateMode PostUpdateMode)
{
    FSavedMove_Character::PostUpdate(C, PostUpdateMode);

    // ئەگەر لە ناو UpdateCharacterStateBeforeMovement دۆخێک بوەستێندرێت، لێرەدا فلاجەکە بۆ ناردن نوێ دەبێتەوە
    if (PostUpdateMode == PostUpdate_Record)
    {
        if (const UHamaMovementComponent* Comp = static_cast<const UHamaMovementComponent*>(C->GetCharacterMovement()))
        {
            bSavedWantsToSprint = Comp->bSprinting;
            bSavedWantsToAim = Comp->bAiming;
            bSavedWantsToDive = Comp->bDiving;
            bSavedWantsToSlide = Comp->bSlide;
        }
    }
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
    if (bSavedWantsToAim != Other->bSavedWantsToAim)       return false;
    if (bSavedWantsToDive != Other->bSavedWantsToDive)     return false;
    if (bSavedWantsToSlide != Other->bSavedWantsToSlide)   return false;
    if (bSavedWasSliding != Other->bSavedWasSliding)       return false;
    if (bSavedWasDiving != Other->bSavedWasDiving)         return false;

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