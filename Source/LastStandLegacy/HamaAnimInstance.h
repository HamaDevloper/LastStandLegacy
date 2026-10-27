#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "HamaAnimInstance.generated.h"

class AHama;
class UHamaMovementComponent;
class ABaseWeapon;
class UAnimSequence;
class UAimOffsetBlendSpace1D;

UCLASS(Transient, Blueprintable)
class LASTSTANDLEGACY_API UHamaAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

    void SetEquippedWeapon(ABaseWeapon* NewWeapon);

protected:
    // =========================================================================
    // 1. Pointers & References (UE5 TObjectPtr Standard)
    // =========================================================================

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Hama|References")
    TObjectPtr<AHama> HamaCharacter = nullptr;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Hama|References")
    TObjectPtr<UHamaMovementComponent> MovementComponent = nullptr;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Hama|Weapon")
    TObjectPtr<ABaseWeapon> EquippedWeapon = nullptr;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Hama|Weapon")
    TObjectPtr<UAnimSequence> CurrentWeaponIdle = nullptr;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Hama|Weapon")
    TObjectPtr<UAnimSequence> CurrentWeaponSprint = nullptr;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Hama|Weapon")
    TObjectPtr<UAnimSequence> CurrentWeaponAim = nullptr;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Hama|Weapon")
    TObjectPtr<UAimOffsetBlendSpace1D> CurrentAimOffset = nullptr;

    // =========================================================================
    // 2. Vectors & Rotators (Aligned together for CPU Cache efficiency)
    // =========================================================================

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Hama|Movement")
    FVector PlayerVelocity = FVector::ZeroVector;

    FRotator CachedActorRotation = FRotator::ZeroRotator;
    FRotator CachedAimRotation = FRotator::ZeroRotator;

    // =========================================================================
    // 3. Floats (4-Byte Aligned)
    // =========================================================================

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Hama|Movement")
    float GroundSpeed = 0.f;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Hama|Movement")
    float Direction = 0.f;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Hama|Aim")
    float Pitch = 0.f;

    // =========================================================================
    // 4. Booleans (Packed at the end to prevent struct padding / memory holes)
    // =========================================================================

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Hama|Movement")
    bool bShouldMove = false;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Hama|States")
    bool bIsSprinting = false;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Hama|States")
    bool bIsFalling = false;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Hama|States")
    bool bIsDowned = false;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Hama|States")
    bool bIsAiming = false;

    bool bCachedHasAcceleration = false;
    bool bCachedWantsToSprint = false;
};