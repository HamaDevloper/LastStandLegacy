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
    virtual void BeginPlay() override;
    virtual float GetMaxSpeed() const override;
    virtual void UpdateFromCompressedFlags(uint8 Flags) override;
    virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
    virtual FVector ScaleInputAcceleration(const FVector& InputAcceleration) const override;

    uint8 bSprinting : 1;
    uint8 bAiming : 1;
    uint8 bDiving : 1;
    uint8 bDowned : 1;
    uint8 bSlide : 1;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement Speeds")
    float SprintSpeed = 800.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement Speeds")
    float AimSpeed = 150.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement Speeds")
    float DownSpeed = 50.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement Speeds")
    float SlideSpeed = 650.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Movement | Dive")
    float DiveImpulseHorizontal = 900.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Movement | Dive")
    float DiveImpulseVertical = 350.f;

    float DefaultGroundFriction;
    float DefaultBrakingDecelerationWalking;

private:
    bool bWasSliding = false;
    bool bWasDiving = false;
    bool bWasSprinting = false;

    TWeakObjectPtr<class UHamaComponent> CachedHamaComp;
    UHamaComponent* GetHamaComp();

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