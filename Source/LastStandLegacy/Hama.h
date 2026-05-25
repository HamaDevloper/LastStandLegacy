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
class ABaseWeapon;
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

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public:
	// -----------------------------------------------------------------------------
	// Components & References (پێکهاتەکان و سەرچاوەکان)
	// -----------------------------------------------------------------------------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hama|Components")
	TObjectPtr<UHamaComponent> HamaComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hama|Components")
	TObjectPtr<UHamaMovementComponent> HamaMovementComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hama|Components")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hama|Components")
	TObjectPtr<UCameraComponent> TPCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hama|Components")
	TObjectPtr<UCameraComponent> FPCamera;

	UPROPERTY(BlueprintReadOnly, Category = "Hama|References")
	TObjectPtr<APlayerController> OwnerController;

public:
	// -----------------------------------------------------------------------------
	// Weapons & Inventory (چەک و جبەخانە)
	// -----------------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Weapons")
	TSubclassOf<ABaseWeapon> DefaultWeapon;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hama|Weapons")
	TObjectPtr<ABaseWeapon> CurrentWeapon;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hama|Weapons")
	TObjectPtr<ABaseWeapon> PrimaryWeapon;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hama|Weapons")
	TObjectPtr<ABaseWeapon> SecondaryWeapon;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hama|Weapons")
	TObjectPtr<ABaseWeapon> ThirdWeapon;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hama|Weapons")
	FName SocketName;

public:
	// -----------------------------------------------------------------------------
	// Input Mapping & Actions (ڕێکخستنەکانی ئینپوت)
	// -----------------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Input")
	TObjectPtr<UInputAction> SwitchCameraAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Input")
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Input")
	TObjectPtr<UInputAction> CrouchAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Input")
	TObjectPtr<UInputAction> AimAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Input")
	TObjectPtr<UInputAction> FireAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Input|Sensitivity")
	float NormalSensitivity = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Input|Sensitivity")
	float AimingSensitivity = 0.5f;

public:
	// -----------------------------------------------------------------------------
	// UI & HUD (ڕووکاری بەکارهێنەر)
	// -----------------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|UI")
	TSubclassOf<UUserWidget> PlayerCrossHairClass;

	UPROPERTY(BlueprintReadOnly, Category = "Hama|UI")
	TObjectPtr<UUserWidget> CrossHairRef;

public:
	// -----------------------------------------------------------------------------
	// State Checking & Logic Functions (فەنکشنەکانی پشکنینی دۆخ)
	// -----------------------------------------------------------------------------
	UPROPERTY(BlueprintReadOnly, Category = "Hama|State")
	bool bIsCrouchButtonHold = false;

	UPROPERTY(BlueprintReadOnly, Category = "Hama|State")
	bool bCanJumpSlide = false;

	UFUNCTION(BlueprintCallable, Category = "Hama|Perks")
	void StaminaUpPerk();

	UFUNCTION(Server, Reliable)
	void Server_StaminaUpPerk();

	bool IsAimButtonHeld() const { return bIsAimButtonHold; }

public:
	// -----------------------------------------------------------------------------
	// Animations (ئەنیمەیشنەکان)
	// -----------------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hama|Animations")
	UAnimMontage* SlideMontage;

	void PlayDrinkPerkAnimation(UAnimMontage* PerkMontageToPlay);

	UFUNCTION(Server, Reliable)
	void Server_PlayDrinkPerkAnimation(UAnimMontage* PerkMontageToPlay);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayDrinkPerkAnimation(UAnimMontage* PerkMontageToPlay);

public:
	// -----------------------------------------------------------------------------
	// Blueprint Events (ڕووداوەکانی بلووپڕینت)
	// -----------------------------------------------------------------------------
	UFUNCTION(BlueprintImplementableEvent, Category = "Hama|Events")
	void CrossHairUpdate(bool bInRange);

	UFUNCTION(BlueprintImplementableEvent, Category = "Hama|Events")
	void OnAim(bool InAiming);

protected:
	// -----------------------------------------------------------------------------
	// Protected Lifecycle Overrides
	// -----------------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;

protected:
	// -----------------------------------------------------------------------------
	// Protected Camera & Animations
	// -----------------------------------------------------------------------------
	UPROPERTY(BlueprintReadOnly, Category = "Hama|Camera")
	bool bIsInFirstPerson = false;

	UPROPERTY(BlueprintReadOnly, Category = "Hama|Camera")
	bool bIsInRightShoulderView = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hama|Animations")
	UAnimMontage* DrinkPerkMontage;

	UFUNCTION(BlueprintImplementableEvent, Category = "Hama|Events")
	void Switchcamera(bool bIsRightShoulderViewChanged);

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
	void CreateDefaultWeapon();
	

protected:
	// -----------------------------------------------------------------------------
	// Protected Internal Variables
	// -----------------------------------------------------------------------------
	static const float CrossHairTimer;

	bool bIsHoldedTrigger = false;
	bool bIsAimButtonHold = false;

private:
	// -----------------------------------------------------------------------------
	// Private Core Logic
	// -----------------------------------------------------------------------------
	FTimerHandle CrossHairTimerHandle;
	bool bLastCrossHairState = false;

	bool IsSprinting() const;
};