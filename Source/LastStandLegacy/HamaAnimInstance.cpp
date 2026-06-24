#include "HamaAnimInstance.h"
#include "Hama.h"
#include "BaseWeapon.h"
#include "GameFramework/CharacterMovementComponent.h"

void UHamaAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    HamaCharacter = Cast<AHama>(TryGetPawnOwner());
    if (!HamaCharacter) return;

    MovementComponent = HamaCharacter->GetCharacterMovement();
    HamaCharacter->OnWeaponChanged.AddUObject(this, &UHamaAnimInstance::OnWeaponChanged);
    
    if (HamaCharacter->CurrentWeapon)
    {
        OnWeaponChanged(HamaCharacter->CurrentWeapon);
    }
}

void UHamaAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    if (!HamaCharacter)
    {
        HamaCharacter = Cast<AHama>(TryGetPawnOwner());
        if (!HamaCharacter) return;

        MovementComponent = HamaCharacter->GetCharacterMovement();
    }

    if (!MovementComponent) return;

    PlayerVelocity = HamaCharacter->GetVelocity();

    FVector Velocity = HamaCharacter->GetVelocity();
    Velocity.Z = 0.f;
    float SpeedSq = Velocity.SizeSquared();

    GroundSpeed = FMath::Sqrt(SpeedSq);

    bShouldMove = (SpeedSq > 9.f)
        && !MovementComponent->GetCurrentAcceleration().IsNearlyZero();

    bIsFalling = MovementComponent->IsFalling();

    FRotator Rotation = HamaCharacter->GetActorRotation();
    FVector Forward = Rotation.Vector();
    FVector Right = FRotationMatrix(Rotation).GetScaledAxis(EAxis::Y);

    float ForwardDot = FVector::DotProduct(Velocity.GetSafeNormal(), Forward);
    float RightDot = FVector::DotProduct(Velocity.GetSafeNormal(), Right);
    Direction = FMath::RadiansToDegrees(FMath::Atan2(RightDot, ForwardDot));

    bIsAiming = HamaCharacter->IsAiming();
    bIsSprinting = HamaCharacter->IsSprinting();
}

void UHamaAnimInstance::OnWeaponChanged(ABaseWeapon* NewWeapon)
{
    if (NewWeapon)
    {
        CurrentWeaponIdle = NewWeapon->GetWeaponIdle();
        CurrentWeaponSprint = NewWeapon->GetWeaponSprint();
        CurrentWeaponAim = NewWeapon->GetAimMontage();
    }
    else
    {
        CurrentWeaponIdle = nullptr;
        CurrentWeaponSprint = nullptr;
        CurrentWeaponAim = nullptr;
    }
}