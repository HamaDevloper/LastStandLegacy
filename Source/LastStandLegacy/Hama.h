// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HamaComponent.h"
#include "BaseWeapon.h"
#include "Hama.generated.h"

#define ECC_Bullet ECC_GameTraceChannel1
#define ECC_CrossHair ECC_GameTraceChannel2


DECLARE_MULTICAST_DELEGATE_OneParam(FOnWeaponChanged, ABaseWeapon*);
DECLARE_DELEGATE_OneParam(FOnAimChanged, bool);
DECLARE_DELEGATE_OneParam(FOnSprintChanged, bool);

// -----------------------------------------------------------------------------
// Forward Declarations
// -----------------------------------------------------------------------------

class UHamaMovementComponent;
class UHamaAbilityComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class APlayerController;
class AZombie;

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
    TObjectPtr<UHamaAbilityComponent> HamaAbilityComponent;

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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama | UI")
    TSubclassOf<class UHamaMainWidget> MainWidgetClass;

    UPROPERTY(BlueprintReadOnly, Category = "Hama | UI")
    class UHamaMainWidget* MainWidgetRef;

protected:
    // فەنکشنی بنەڕەتی ئینجین کە خۆی چاوەڕێی گەیشتنی ڕاستەقینەی PlayerState دەکات
    virtual void OnPlayerStateChanged(APlayerState* NewPlayerState, APlayerState* OldPlayerState) override;

    // لۆجیکی نێوخۆیی بۆ بەستنەوەی دیسپاچەرەکان
    void BindPlayerStateEvents();

    // فەنکشنەکان کە کاتێک دیسپاچەری C++ لێدەدات، ئەمان بەئاگا دێنەوە
    UFUNCTION()
    void HandlePointsChanged(int32 NewPoints);

    UFUNCTION()
    void HandleKillsChanged(int32 NewKills);

    UFUNCTION()
    void UpdatePingUI();

    FTimerHandle PingUpdateTimerHandle;

    // ئەمەش ئیڤێنتێکە بۆ ناو بلوپرینت (UI) تا تەنها تێکستەکە ئەپدیت بکاتەوە بێ کاست
    UFUNCTION(BlueprintImplementableEvent, Category = "Hama | UI")
    void OnUIUpdatePoints(int32 NewPoints);

    UFUNCTION(BlueprintImplementableEvent, Category = "Hama | UI")
    void OnUIUpdateKills(int32 NewKills);

    UFUNCTION(BlueprintImplementableEvent, Category = "Hama | UI")
    void OnUIUpdatePing(int32 NewKills);

public:
    // -----------------------------------------------------------------------------
    // State Checking & Logic Functions
    // -----------------------------------------------------------------------------
    UPROPERTY(BlueprintReadOnly, Category = "Hama|State")
    bool bIsCrouchButtonHold = false;

    UPROPERTY(BlueprintReadOnly, Category = "Hama|State")
    bool bCanJumpSlide = false;

    bool IsAimButtonHold() const { return bIsAimButtonHold; }
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
    // Health
    // -----------------------------------------------------------------------------
public:
    UPROPERTY(ReplicatedUsing = OnRep_Health, EditAnywhere, BlueprintReadwrite, Category = "Hama|Health")
    float CurrentHealth;

    UPROPERTY(ReplicatedUsing = OnRep_Health, EditAnywhere, BlueprintReadwrite, Category = "Hama|Health")
    float MaxHealth = 100.f;

    UFUNCTION()
    void OnRep_Health();

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
    void AimPressedSitck();

protected:
    static const float CrossHairTimer;

    bool bIsHoldedTrigger = false;
    bool bIsAimButtonHold = false;

    // --- Aim Assist Settings ---
    UPROPERTY()
    TWeakObjectPtr<AZombie> LockedTarget;

    bool bIsStickyAiming = false;


    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hama|Targeting")
    float StickySlowdownMultiplier = 0.5f; // خاوبوونەوەی سێنسەتیڤیتی کاتی قوفڵبوون

public:
    bool bIsFireButtonHold = false;

private:
    FTimerHandle CrossHairTimerHandle;
    bool bLastCrossHairState = false;

public:
       FORCEINLINE bool IsSprinting() const { return HamaComponent && HamaComponent->bIsSprinting; }
       FORCEINLINE bool IsAiming() const { return HamaComponent && HamaComponent->bIsAiming; }

protected:
    // CameraSensitivity
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Input|Sensitivity")
    float NormalSensitivity = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Input|Sensitivity")
    float AimingSensitivity = 0.5f;

public:
    FOnWeaponChanged OnWeaponChanged;
    FOnAimChanged OnAimChanged;
    FOnSprintChanged OnSprintChanged;
};