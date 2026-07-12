#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HamaMovementComponent.generated.h"

UCLASS()
class LASTSTANDLEGACY_API UHamaMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    UHamaMovementComponent();
    virtual float GetMaxSpeed() const override;

protected:
    virtual void UpdateFromCompressedFlags(uint8 Flags) override;
    virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

public:
    // ================= MOVEMENT SPEEDS =================
    UPROPERTY(EditDefaultsOnly, Category = "Hama|Movement")
    float SprintSpeed = 800.f;

    UPROPERTY(EditDefaultsOnly, Category = "Hama|Movement")
    float AimSpeed = 300.f;

    UPROPERTY(EditDefaultsOnly, Category = "Hama|Movement")
    float DownSpeed = 100.f;

    // ================= SLIDE PHYSICS =================
    UPROPERTY(EditDefaultsOnly, Category = "Hama|Movement|Slide")
    float SlideSpeed = 650.f;

    UPROPERTY(EditDefaultsOnly, Category = "Hama|Movement|Slide")
    float SlideImpulse = 800.f;

    UPROPERTY(EditDefaultsOnly, Category = "Hama|Movement|Slide")
    float SlideFriction = 0.1f;

    // ================= STATE FLAGS =================
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    uint8 bSprinting : 1;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    uint8 bAiming : 1;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    uint8 bDowned : 1;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    uint8 bSlide : 1; // 🚀 زیادکرا بۆ خلیسکاندن بەبێ لاگ

    // ================= SAVED MOVE =================
    class FSavedMove_Hama : public FSavedMove_Character
    {
        typedef FSavedMove_Character Super;

    public:
        uint8 bSavedWantsToSprint : 1;
        uint8 bSavedWantsToAim : 1;
        uint8 bSavedIsDowned : 1;
        uint8 bSavedWantsToSlide : 1;

        FSavedMove_Hama() : bSavedWantsToSprint(0), bSavedWantsToAim(0), bSavedIsDowned(0), bSavedWantsToSlide(0) {}

        virtual void Clear() override;
        virtual void SetMoveFor(ACharacter* C, float DT, FVector const& Accel, FNetworkPredictionData_Client_Character& Data) override;
        virtual void PrepMoveFor(ACharacter* C) override;
        virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* C, float MaxDelta) const override;
        virtual uint8 GetCompressedFlags() const override;
    };

    // ================= PREDICTION DATA =================
    class FNetworkPredictionData_Client_Hama : public FNetworkPredictionData_Client_Character
    {
        typedef FNetworkPredictionData_Client_Character Super;

    public:
        explicit FNetworkPredictionData_Client_Hama(const UCharacterMovementComponent& MoveComponent);
        virtual FSavedMovePtr AllocateNewMove() override;
    };
};