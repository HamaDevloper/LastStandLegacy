// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Hama.generated.h"

#define ECC_Zombie ECC_GameTraceChannel1
// -----------------------------------------------------------------------------
// Forward Declarations (پێناسە پێشوەختەکان)
// -----------------------------------------------------------------------------
class UHamaComponent;
class UHamaMovementComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class APlayerController;

struct FInputActionValue;
struct FInputActionInstance;

UCLASS()
class LASTSTANDLEGACY_API AHama : public ACharacter
{
	GENERATED_BODY()

public:
	// -----------------------------------------------------------------------------
	// Lifecycle (سوڕی ژیانی ئەکتەرەکە)
	// -----------------------------------------------------------------------------
	AHama(const FObjectInitializer& ObjectInitializer);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void PossessedBy(AController* NewController) override; // بۆ سێرڤەر
	virtual void OnRep_Controller() override;
public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public:
	// -----------------------------------------------------------------------------
	// Components (پێکهاتەکانی کارەکتەرەکە)
	// -----------------------------------------------------------------------------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Components")
	TObjectPtr<UHamaComponent> HamaComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Components")
	TObjectPtr<UHamaMovementComponent> HamaMovementComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Components")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Components")
	TObjectPtr<UCameraComponent> TPCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Components")
	TObjectPtr<UCameraComponent> FPCamera;

	UPROPERTY(BlueprintReadOnly, Category = "Player|References")
	TObjectPtr<APlayerController> OwnerController;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player|UI")
	TSubclassOf<UUserWidget> PlayerCrossHairClass;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	TObjectPtr<UUserWidget> CrossHairRef;

public:
	// -----------------------------------------------------------------------------
	// Input Mapping & Actions (ڕێکخستنەکانی ئینپوت)
	// -----------------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> SwitchCameraAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> CrouchAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> AimAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> FireAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player|Sensitivity")
	float NormalSensitivity = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player|Sensitivity")
	float AimingSensitivity = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player|Camera")
	bool bIsAiming = false;

protected:
	// -----------------------------------------------------------------------------
	// Internal State Variables (گۆڕاوە ناوخۆییەکانی دۆخی کارەکتەر)
	// -----------------------------------------------------------------------------
	static const float CrossHairTimer;
	
	UPROPERTY(BlueprintReadOnly, Category = "Player|Camera")
	bool bIsInFirstPerson = false;

	UPROPERTY(BlueprintReadOnly, Category = "Player|Camera")
	bool bIsInRightShoulderView = false;

	bool bIsHoldedTrigger = false;
	bool bIsAimButtonHold = false;
	UPROPERTY(BlueprintReadOnly, Category = "Player|Camera")
	bool bIsCrouchButtonHold = false;
	UPROPERTY(BlueprintReadOnly, Category = "Player|Camera")
	bool bCanJumpSlide = false;

public:
	UFUNCTION(BlueprintCallable, Category = "Player|Perks")
	void StaminaUpPerk();

	// RPC بۆ ئەوەی داواکارییەکە بنێرێت بۆ سێرڤەر
	UFUNCTION(Server, Reliable)
	void Server_StaminaUpPerk();

	bool IsAimButtonHeld() const { return bIsAimButtonHold; }

protected:
	// -----------------------------------------------------------------------------
	// Input Callbacks & Events (فەنکشنەکانی ئینپوت)
	// -----------------------------------------------------------------------------
	void FireActionPressed();
	void FireActionReleased();
	void AimActionPressed();
	void AimActionReleased();
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void JumpActionPressed();
	void CrouchActionPressed();
	void CrouchActionReleased();
    void StartSlideRoutine();
    void StopSlideRoutine();
	void SwitchCameraPressed(const FInputActionInstance& Instance);
	void SwitchCameraReleased();
	void SprintActionPressed();
	void StartCrossHairTimer();
	void CrossHairTrace();
	void OnCrossHairTraceCompleted(const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum);

	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);


	UFUNCTION(BlueprintImplementableEvent, Category = "Player|Event")
	void Switchcamera(bool bIsRightShoulderViewChanged);

public:
	UFUNCTION(BlueprintImplementableEvent, Category = "Player|Event")
	void OnAim(bool InAiming);

	UFUNCTION(BlueprintImplementableEvent, Category = "Player|Event")
	void CrossHairUpdate(bool bInRange);

protected:
	bool IsSprinting() const;

protected:
	// ئەنیمەیشنی پەرکەکە لە جۆری Anim Montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations")
	UAnimMontage* DrinkPerkMontage;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations")
	UAnimMontage* SlideMontage;

public:
	// ئەمە فەنکشنە سەرەکییەکەیە کە لۆکاڵی لێدەدات
	void PlayDrinkPerkAnimation(UAnimMontage* PerkMontageToPlay);

	// ئەمە فەنکشنی سێرڤەرە بۆ ئەوەی یاریزانەکانی تر ئاگادار بکاتەوە
	UFUNCTION(Server, Reliable)
	void Server_PlayDrinkPerkAnimation(UAnimMontage* PerkMontageToPlay);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayDrinkPerkAnimation(UAnimMontage* PerkMontageToPlay);



private:
	FTimerHandle CrossHairTimerHandle;

	bool bLastCrossHairState = false;

};