// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Hama.generated.h"

#define ECC_Zombie ECC_GameTraceChannel1

// -----------------------------------------------------------------------------
// Forward Declarations
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
	AHama(const FObjectInitializer& ObjectInitializer);
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public:
	// -----------------------------------------------------------------------------
	// Components & References
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
	// Weapons & Inventory
	// -----------------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Weapons")
	TSubclassOf<ABaseWeapon> DefaultWeapon;

	// لادانی COND_SkipOwner بۆ ئەوەی کڵایەنتی خاوەنیش چەکەکەی پێبگات
	UPROPERTY(ReplicatedUsing = OnRep_CurrentWeapon, BlueprintReadOnly, Category = "Hama|Weapons")
	TObjectPtr<ABaseWeapon> CurrentWeapon;

	UPROPERTY(BlueprintReadOnly, Category = "Hama|Weapons")
	TObjectPtr<ABaseWeapon> PrimaryWeapon;

	UPROPERTY(BlueprintReadOnly, Category = "Hama|Weapons")
	TObjectPtr<ABaseWeapon> SecondaryWeapon;

	UPROPERTY(BlueprintReadOnly, Category = "Hama|Weapons")
	TObjectPtr<ABaseWeapon> ThirdWeapon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Weapons")
	FName SocketName;

public:
	// -----------------------------------------------------------------------------
	// Input Mapping & Actions
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Input")
	TObjectPtr<UInputAction> ReloadAction;

public:
	// -----------------------------------------------------------------------------
	// UI & HUD
	// -----------------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|UI")
	TSubclassOf<UUserWidget> PlayerCrossHairClass;

	UPROPERTY(BlueprintReadOnly, Category = "Hama|UI")
	TObjectPtr<UUserWidget> CrossHairRef;

public:
	// -----------------------------------------------------------------------------
	// State Checking & Logic Functions
	// -----------------------------------------------------------------------------
	UPROPERTY(BlueprintReadOnly, Category = "Hama|State")
	bool bIsCrouchButtonHold = false;

	UPROPERTY(BlueprintReadOnly, Category = "Hama|State")
	bool bCanJumpSlide = false;

	bool IsAimButtonHeld() const { return bIsAimButtonHold; }
	bool IsFireButtonHolded() const { return bIsFireButtonHold; }

public:
	// -----------------------------------------------------------------------------
	// Animations
	// -----------------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hama|Animations")
	UAnimMontage* SlideMontage;

public:
	// -----------------------------------------------------------------------------
	// Blueprint Events
	// -----------------------------------------------------------------------------
	UFUNCTION(BlueprintImplementableEvent, Category = "Hama|Events")
	void CrossHairUpdate(bool bInRange);

	UFUNCTION(BlueprintImplementableEvent, Category = "Hama|Events")
	void OnAim(bool InAiming);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;

	// فەنکشن بۆ کاتێک چەکەکە لە سێرڤەرەوە دەگاتە کڵایەنت بۆ ئەوەی Attach بێت
	UFUNCTION()
	void OnRep_CurrentWeapon();

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Hama|Camera")
	bool bIsInFirstPerson = false;

	UPROPERTY(BlueprintReadOnly, Category = "Hama|Camera")
	bool bIsInRightShoulderView = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hama|Animations")
	UAnimMontage* DrinkPerkMontage;

	UFUNCTION(BlueprintImplementableEvent, Category = "Hama|Events")
	void Switchcamera(bool bIsRightShoulderViewChanged);


	// -----------------------------------------------------------------------------
	// Input Callbacks & Network RPCs
	// -----------------------------------------------------------------------------
public:
	void FireActionPressed();
protected:
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
	void AttachWeaponToMesh(ABaseWeapon* WeaponToAttach);
	void ReloadActionPressed();

protected:
	static const float CrossHairTimer;

	bool bIsHoldedTrigger = false;
	bool bIsAimButtonHold = false;
public:
	bool bIsFireButtonHold = false;

private:
	FTimerHandle CrossHairTimerHandle;
	bool bLastCrossHairState = false;

	bool IsSprinting() const;


protected:
	// CameraSensitivity
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Input|Sensitivity")
	float NormalSensitivity = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Input|Sensitivity")
	float AimingSensitivity = 0.5f;
};