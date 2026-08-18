#include "RecoilComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

URecoilComponent::URecoilComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;

    CurrentRecoil = FRotator::ZeroRotator;
    TargetRecoil = FRotator::ZeroRotator;
    LastFireTime = 0.f;
    RecoilRecoverySpeed = 15.0f;
}

void URecoilComponent::BeginPlay()
{
    Super::BeginPlay();

    OwnerController = Cast<APlayerController>(GetOwner());
}

void URecoilComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!OwnerController) return;

    if (GetWorld()->GetTimeSeconds() - LastFireTime > RecoilRecoveryDelay)
    {
        TargetRecoil = FMath::RInterpTo(TargetRecoil, FRotator::ZeroRotator, DeltaTime, RecoilReturnSpeed);
    }

    FRotator PreviousRecoil = CurrentRecoil;
    CurrentRecoil = FMath::RInterpTo(CurrentRecoil, TargetRecoil, DeltaTime, RecoilRecoverySpeed);

    float DeltaPitch = CurrentRecoil.Pitch - PreviousRecoil.Pitch;
    float DeltaYaw = CurrentRecoil.Yaw - PreviousRecoil.Yaw;

    OwnerController->AddPitchInput(-DeltaPitch);
    OwnerController->AddYawInput(DeltaYaw);

    if (CurrentRecoil.IsNearlyZero(0.01f) && TargetRecoil.IsNearlyZero(0.01f))
    {
        CurrentRecoil = FRotator::ZeroRotator;
        TargetRecoil = FRotator::ZeroRotator;
        SetComponentTickEnabled(false);
    }
}

void URecoilComponent::AddRecoil(UCurveFloat* RecoilPitchCurve, int32 ShotsFired, float Multiplier, float Randomness, float RecoverySpeed)
{
    if (!OwnerController || !OwnerController->IsLocalController()) return;

    LastFireTime = GetWorld()->GetTimeSeconds();
    RecoilRecoverySpeed = RecoverySpeed;

    if (RecoilPitchCurve)
    {
        float PitchKick = RecoilPitchCurve->GetFloatValue(ShotsFired) * Multiplier;
        float YawKick = FMath::RandRange(-Randomness, Randomness) * Multiplier;

        PitchKick += FMath::RandRange(-Randomness * 0.25f, Randomness * 0.25f) * Multiplier;

        TargetRecoil += FRotator(PitchKick, YawKick, 0.f);

        SetComponentTickEnabled(true);
    }
}

void URecoilComponent::ResetRecoil()
{
    CurrentRecoil = FRotator::ZeroRotator;
    TargetRecoil = FRotator::ZeroRotator;
    SetComponentTickEnabled(false);
}