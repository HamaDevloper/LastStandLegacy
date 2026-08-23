#include "HamaAnimInstance.h"
#include "Hama.h"
#include "BaseWeapon.h"
#include "HamaMovementComponent.h"

void UHamaAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    HamaCharacter = Cast<AHama>(TryGetPawnOwner());
    if (HamaCharacter)
    {
        MovementComponent = Cast<UHamaMovementComponent>(HamaCharacter->GetCharacterMovement());
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

    ABaseWeapon* TargetWeapon = HamaCharacter->GetCurrentWeapon();
    if (EquippedWeapon != TargetWeapon)
    {
        SetEquippedWeapon(TargetWeapon);
    }

    CachedActorRotation = HamaCharacter->GetActorRotation();
    
    const FRotator AimRotation = HamaCharacter->GetBaseAimRotation();
    const FRotator DeltaRot = (AimRotation - CachedActorRotation).GetNormalized();
    CachedPitch = DeltaRot.Pitch;

    PlayerVelocity = HamaCharacter->GetVelocity();
    bCachedHasAcceleration = !MovementComponent->GetCurrentAcceleration().IsNearlyZero();

    bIsSprinting = MovementComponent->bSprinting;
    bIsFalling = MovementComponent->IsFalling();
    bIsDowned = MovementComponent->bDowned;
}

void UHamaAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

    Pitch = FMath::Clamp(CachedPitch, -90.f, 90.f);

    const FVector2D Velocity2D(PlayerVelocity.X, PlayerVelocity.Y);
    const float SpeedSq = Velocity2D.SizeSquared();

    GroundSpeed = FMath::Sqrt(SpeedSq);
    bShouldMove = (SpeedSq > 9.f) && bCachedHasAcceleration;

    if (bShouldMove)
    {
        float SinYaw, CosYaw;
        FMath::SinCos(&SinYaw, &CosYaw, FMath::DegreesToRadians(CachedActorRotation.Yaw));

        const FVector2D Forward2D(CosYaw, SinYaw);
        const FVector2D Right2D(-SinYaw, CosYaw);

        const FVector2D NormalVelocity = Velocity2D.GetSafeNormal();
        const float ForwardDot = FVector2D::DotProduct(NormalVelocity, Forward2D);
        const float RightDot = FVector2D::DotProduct(NormalVelocity, Right2D);

        Direction = FMath::RadiansToDegrees(FMath::Atan2(RightDot, ForwardDot));
    }
    else
    {
        Direction = 0.f;
    }
}