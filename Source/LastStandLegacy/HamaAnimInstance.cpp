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

// --------------------------------------------------------------------------------------
// GAME THREAD (تەنها بۆ خوێندنەوەی سەلامەت لە Actor و Component)
// --------------------------------------------------------------------------------------
void UHamaAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    if (!HamaCharacter)
    {
        HamaCharacter = Cast<AHama>(TryGetPawnOwner());
        if (!HamaCharacter) return;
        MovementComponent = Cast<UHamaMovementComponent>(HamaCharacter->GetCharacterMovement());
    }

    if (!MovementComponent) return;

    if (GEngine)
    {
        FString Msg = FString::Printf(TEXT("[%s] AnimTick Weapon=%s"),
            *HamaCharacter->GetName(),
            HamaCharacter->CurrentWeapon ? *HamaCharacter->CurrentWeapon->GetName() : TEXT("NULL"));
        GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::Cyan, Msg);
    }

    if (HamaCharacter->CurrentWeapon != LastKnownWeapon)
    {
        LastKnownWeapon = HamaCharacter->CurrentWeapon;
        OnWeaponChanged(LastKnownWeapon);
    }

    PlayerVelocity = HamaCharacter->GetVelocity();
    CachedActorRotation = HamaCharacter->GetActorRotation();
    bCachedHasAcceleration = !MovementComponent->GetCurrentAcceleration().IsNearlyZero();

    bIsSprinting = MovementComponent->bSprinting;
    bIsFalling = MovementComponent->IsFalling();
    bIsDowned = MovementComponent->bDowned;
}

// --------------------------------------------------------------------------------------
// WORKER THREAD (هەموو لێکدانەوەیەکی بیرکاری لێرە دەکرێت بەبێ ڕاگرتنی یارییەکە)
// لێرەدا قەدەغەیە بنووسیت HamaCharacter->... یان MovementComponent->...
// --------------------------------------------------------------------------------------
void UHamaAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

    FVector Velocity2D = PlayerVelocity;
    Velocity2D.Z = 0.f;
    float SpeedSq = Velocity2D.SizeSquared();

    GroundSpeed = FMath::Sqrt(SpeedSq);
    bShouldMove = (SpeedSq > 9.f) && bCachedHasAcceleration;

    FVector Forward = CachedActorRotation.Vector();
    FVector Right = FRotationMatrix(CachedActorRotation).GetScaledAxis(EAxis::Y);

    FVector NormalVelocity = Velocity2D.GetSafeNormal();
    float ForwardDot = FVector::DotProduct(NormalVelocity, Forward);
    float RightDot = FVector::DotProduct(NormalVelocity, Right);

    Direction = FMath::RadiansToDegrees(FMath::Atan2(RightDot, ForwardDot));
}

void UHamaAnimInstance::OnWeaponChanged(ABaseWeapon* NewWeapon)
{
    if (GEngine)
    {
        FString Msg = FString::Printf(TEXT("OnWeaponChanged CALLED: %s"),
            NewWeapon ? *NewWeapon->GetName() : TEXT("NULL"));
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, Msg);
    }
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