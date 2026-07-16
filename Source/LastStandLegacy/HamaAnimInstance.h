#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "HamaAnimInstance.generated.h"

class AHama;
class ABaseWeapon;
class UAnimSequence;
class UHamaMovementComponent;

UCLASS()
class LASTSTANDLEGACY_API UHamaAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

private:
    UPROPERTY()
    TObjectPtr<AHama> HamaCharacter;

    UPROPERTY()
    TObjectPtr<UHamaMovementComponent> MovementComponent;

    UPROPERTY()
    TObjectPtr<ABaseWeapon> LastKnownWeapon;

    FRotator CachedActorRotation;
    bool bCachedHasAcceleration;

public:
    UFUNCTION()
    void OnWeaponChanged(ABaseWeapon* NewWeapon);

protected:
    // ── Weapon Data ───────────────────────────────
    UPROPERTY(BlueprintReadOnly, Category = "Weapon Data")
    TObjectPtr<UAnimSequence> CurrentWeaponIdle;

    UPROPERTY(BlueprintReadOnly, Category = "Weapon Data")
    TObjectPtr<UAnimSequence> CurrentWeaponSprint;

    UPROPERTY(BlueprintReadOnly, Category = "Weapon Data")
    TObjectPtr<UAnimSequence> CurrentWeaponAim;

    // ── State Data ────────────────────────────────
    UPROPERTY(BlueprintReadOnly, Category = "State Data")
    bool bIsSprinting = false;

    UPROPERTY(BlueprintReadOnly, Category = "State Data")
    bool bIsAiming = false;

    UPROPERTY(BlueprintReadOnly, Category = "State Data")
    bool bIsDowned = false;

    UPROPERTY(BlueprintReadOnly, Category = "State Data")
    FVector PlayerVelocity;

    // ── Movement Data ─────────────────────────────
    UPROPERTY(BlueprintReadOnly, Category = "Movement Data")
    float GroundSpeed = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Movement Data")
    bool bShouldMove = false;

    UPROPERTY(BlueprintReadOnly, Category = "Movement Data")
    bool bIsFalling = false;

    UPROPERTY(BlueprintReadOnly, Category = "Movement Data")
    float Direction = 0.f;
};