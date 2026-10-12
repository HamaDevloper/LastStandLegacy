#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HamaMovementComponent.generated.h"

class UHamaComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LASTSTANDLEGACY_API UHamaMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    UHamaMovementComponent();

    virtual void BeginPlay() override;
    virtual float GetMaxSpeed() const override;
    virtual void UpdateFromCompressedFlags(uint8 Flags) override;
    virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
    virtual FVector ScaleInputAcceleration(const FVector& InputAcceleration) const override;
    virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;

    // -------------------------------------------------------------------------
    // Movement States (Accessible in Blueprints / AnimInstance)
    // -------------------------------------------------------------------------
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement | State")
    uint8 bSprinting : 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement | State")
    uint8 bAiming : 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement | State")
    uint8 bDiving : 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement | State")
    uint8 bDowned : 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement | State")
    uint8 bSlide : 1;

    // -------------------------------------------------------------------------
    // Speed Settings
    // -------------------------------------------------------------------------
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement | Speeds")
    float SprintSpeed = 800.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement | Speeds")
    float AimSpeed = 150.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement | Speeds")
    float AimCrouchSpeed = 100.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement | Speeds")
    float DownSpeed = 50.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement | Speeds")
    float SlideSpeed = 650.f;

    // -------------------------------------------------------------------------
    // Slide Physics Settings
    // -------------------------------------------------------------------------
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement | Slide")
    float SlideGroundFriction = 0.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement | Slide")
    float SlideBrakingDeceleration = 200.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement | Slide")
    float SlideStopSpeed = 60.f;

    // -------------------------------------------------------------------------
    // Dive Settings
    // -------------------------------------------------------------------------
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Movement | Dive")
    float DiveImpulseHorizontal = 900.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Movement | Dive")
    float DiveImpulseVertical = 350.f;

protected:
    UPROPERTY(BlueprintReadOnly, Category = "Movement | Defaults")
    float DefaultGroundFriction = 8.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Movement | Defaults")
    float DefaultBrakingDecelerationWalking = 2048.0f;

    UFUNCTION(BlueprintPure, Category = "Movement")
    UHamaComponent* GetHamaComp();

private:
    uint8 bWasSliding : 1;
    uint8 bWasDiving : 1;
    uint8 bPrevSlide : 1;
    uint8 bPrevDiving : 1;
    uint8 bPrevSprinting : 1;

    TWeakObjectPtr<UHamaComponent> CachedHamaComp;

    // -------------------------------------------------------------------------
    // Network Prediction Classes (SavedMove & PredictionData)
    // -------------------------------------------------------------------------
    class FSavedMove_Hama : public FSavedMove_Character
    {
    public:
        uint8 bSavedWantsToSprint : 1;
        uint8 bSavedWantsToAim : 1;
        uint8 bSavedWantsToDive : 1;
        uint8 bSavedWantsToSlide : 1;
        uint8 bSavedWasSliding : 1;
        uint8 bSavedWasDiving : 1;

        virtual void Clear() override;
        virtual uint8 GetCompressedFlags() const override;
        virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* C, float MaxDelta) const override;
        virtual void SetMoveFor(ACharacter* C, float DT, FVector const& Accel, FNetworkPredictionData_Client_Character& Data) override;
        virtual void PrepMoveFor(ACharacter* C) override;
    };

    class FNetworkPredictionData_Client_Hama : public FNetworkPredictionData_Client_Character
    {
    public:
        FNetworkPredictionData_Client_Hama(const UCharacterMovementComponent& MoveComponent);
        virtual FSavedMovePtr AllocateNewMove() override;
    };

public:
    virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
};