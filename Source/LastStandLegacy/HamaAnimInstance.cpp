#include "HamaAnimInstance.h"
#include "Hama.h"
#include "BaseWeapon.h"
#include "HamaMovementComponent.h"
//#include "Animation/AimOffsetBlendSpace1D.h"

void UHamaAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    HamaCharacter = Cast<AHama>(TryGetPawnOwner());
    if (HamaCharacter)
    {
        MovementComponent = Cast<UHamaMovementComponent>(HamaCharacter->GetCharacterMovement());

        if (ABaseWeapon* StartingWeapon = HamaCharacter->GetCurrentWeapon())
        {
            if (StartingWeapon->IsDataInitialized())
            {
                SetEquippedWeapon(StartingWeapon);
            }
        }
    }
}

void UHamaAnimInstance::SetEquippedWeapon(ABaseWeapon* NewWeapon)
{
    EquippedWeapon = NewWeapon;
    if (EquippedWeapon)
    {
        CurrentWeaponIdle = EquippedWeapon->GetWeaponIdle();
        CurrentWeaponSprint = EquippedWeapon->GetWeaponSprint();
        CurrentWeaponAim = EquippedWeapon->GetAimSequence();
        CurrentAimOffset = EquippedWeapon->GetAimOffsetAsset();
    }
    else
    {
        CurrentWeaponIdle = nullptr;
        CurrentWeaponSprint = nullptr;
        CurrentWeaponAim = nullptr;
        CurrentAimOffset = nullptr;
    }
}

void UHamaAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    if (!HamaCharacter)
    {
        HamaCharacter = Cast<AHama>(TryGetPawnOwner());
        if (!HamaCharacter) return;
    }

    if (!MovementComponent)
    {
        MovementComponent = Cast<UHamaMovementComponent>(HamaCharacter->GetCharacterMovement());
        if (!MovementComponent) return;
    }

    CachedActorRotation = HamaCharacter->GetActorRotation();
    CachedAimRotation = HamaCharacter->GetBaseAimRotation();
    PlayerVelocity = HamaCharacter->GetVelocity();

    if (HamaCharacter->GetLocalRole() == ROLE_SimulatedProxy)
    {
        bCachedHasAcceleration = PlayerVelocity.SizeSquared2D() > 9.f;
    }
    else
    {
        bCachedHasAcceleration = !MovementComponent->GetCurrentAcceleration().IsNearlyZero();
    }

    bCachedWantsToSprint = MovementComponent->bSprinting;
    bIsFalling = MovementComponent->IsFalling();
    bIsDowned = MovementComponent->bDowned;
    bIsAiming = MovementComponent->bAiming;
}

void UHamaAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

    const float DeltaPitch = FRotator::NormalizeAxis(CachedAimRotation.Pitch - CachedActorRotation.Pitch);
    Pitch = FMath::Clamp(DeltaPitch, -90.f, 90.f);

    const FVector2D Velocity2D(PlayerVelocity.X, PlayerVelocity.Y);
    const float SpeedSq = Velocity2D.SizeSquared();

    GroundSpeed = FMath::Sqrt(SpeedSq);
    bShouldMove = (SpeedSq > 9.f) && bCachedHasAcceleration;

    bIsSprinting = bCachedWantsToSprint && bShouldMove && !bIsFalling && !bIsDowned;

    if (bShouldMove)
    {
        float SinYaw, CosYaw;
        FMath::SinCos(&SinYaw, &CosYaw, FMath::DegreesToRadians(CachedActorRotation.Yaw));

        const FVector2D Forward2D(CosYaw, SinYaw);
        const FVector2D Right2D(-SinYaw, CosYaw);

        const FVector2D NormalVelocity = Velocity2D / GroundSpeed;
        const float ForwardDot = FVector2D::DotProduct(NormalVelocity, Forward2D);
        const float RightDot = FVector2D::DotProduct(NormalVelocity, Right2D);

        Direction = FMath::RadiansToDegrees(FMath::Atan2(RightDot, ForwardDot));
    }
}