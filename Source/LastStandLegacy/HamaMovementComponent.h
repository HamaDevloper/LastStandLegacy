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

	float SprintSpeed = 800.f;
	float AimSpeed = 150.f;
	float FireSpeed = 300.f;

public:
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	uint8 bSprinting : 1;

	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	uint8 bFiring : 1;

	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	uint8 bAiming : 1;

	// ================= SAVED MOVE =================
	class FSavedMove_Hama : public FSavedMove_Character
	{
		typedef FSavedMove_Character Super;
	public:
		uint8 bSavedWantsToSprint : 1;
		uint8 bSavedWantsToFire : 1;
		uint8 bSavedWantsToAim : 1;

		FSavedMove_Hama() : bSavedWantsToSprint(0), bSavedWantsToFire(0), bSavedWantsToAim(0) {}

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