// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HamaComponent.h"
#include "HealthComponent.h"
#include "BaseWeapon.h"
#include "HamaAbilityComponent.h"
#include "InteractInterface.h"
#include "Hama.generated.h"

#define ECC_Bullet ECC_GameTraceChannel1
#define ECC_CrossHair ECC_GameTraceChannel2
#define ECC_Intract ECC_GameTraceChannel3

//DECLARE_MULTICAST_DELEGATE_OneParam(FOnWeaponChanged, ABaseWeapon*);
DECLARE_DELEGATE_TwoParams(FOnAmmoUpdateDelegate, int32, int32);
DECLARE_DELEGATE_OneParam(FOnInteractUpdateDelegate, const FString&);
DECLARE_DELEGATE_OneParam(FOnCrosshairUpdateDelegate, bool);
DECLARE_DELEGATE_OneParam(FOnPerksChangedSignature, const TArray<FName>& /*CurrentPerks*/);

USTRUCT(BlueprintType)
struct FRoleVisualData
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Role Visuals")
    USkeletalMesh* RoleMesh = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Role Visuals")
    TSubclassOf<UAnimInstance> RoleAnimBP;
};

// -----------------------------------------------------------------------------
// Forward Declarations
// -----------------------------------------------------------------------------
class UHamaMovementComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class APlayerController;
class AZombie;
class ABasePerk;
class USphereComponent;
class UStaticMeshComponent;

struct FInputActionValue;
struct FInputActionInstance;

UCLASS()
class LASTSTANDLEGACY_API AHama : public ACharacter, public IInteractInterface
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
    TObjectPtr<UHealthComponent> HealthComponent;

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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
    TObjectPtr<USphereComponent> InteractSphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> PerkBottleMesh;

public:
    // -----------------------------------------------------------------------------
    // Weapons & Inventory
    // -----------------------------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Weapons")
    TSubclassOf<ABaseWeapon> DefaultWeapon;

    UPROPERTY(ReplicatedUsing = OnRep_CurrentWeapon, BlueprintReadOnly, Category = "Hama|Weapons")
    TObjectPtr<ABaseWeapon> CurrentWeapon;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hama|Weapons")
    TObjectPtr<ABaseWeapon> PrimaryWeapon;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hama|Weapons")
    TObjectPtr<ABaseWeapon> SecondaryWeapon;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hama|Weapons")
    TObjectPtr<ABaseWeapon> ThirdWeapon;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Weapons")
    FName SocketName;

public:
    void RefillSpecificWeaponAmmo(TSubclassOf<ABaseWeapon> WeaponClassToRefill);
    ABaseWeapon* GetWeaponByClass(TSubclassOf<ABaseWeapon> WeaponClassToCheck) const;
protected:
    UPROPERTY(Transient)
    TObjectPtr<ABaseWeapon> PreDeathMachineWeapon;

    FTimerHandle DeathMachineTimerHandle;

public:
    FOnAmmoUpdateDelegate OnAmmoUpdateEvent;
    FOnInteractUpdateDelegate OnInteractUpdateEvent;
    FOnCrosshairUpdateDelegate OnCrosshairUpdateEvent;
    FOnPerksChangedSignature OnPerksChangedEvent;

public:
    UPROPERTY(Transient)
    TObjectPtr<ABaseWeapon> ActiveDeathMachine;

    void GiveDeathMachine(TSubclassOf<ABaseWeapon> WeaponClass, float Duration);

    void RemoveDeathMachine();

    void CompleteWeaponSwap();

    UFUNCTION(BlueprintCallable, Category = "Hama|Weapons")
    void GiveWeapon(TSubclassOf<ABaseWeapon> WeaponClassToGive);

protected:
    UPROPERTY()
    TObjectPtr<ABaseWeapon> PendingWeaponForSwap;

    void Input_SwapWeapon();

    UFUNCTION(BlueprintCallable, Category = "Hama|Weapons")
    void SwapWeapon(ABaseWeapon* TargetWeapon = nullptr);

    UFUNCTION(Server, Reliable)
    void Server_SwapWeapon(ABaseWeapon* NewWeapon);

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlaySwapMontage(float PlayRate);

    UFUNCTION()
    void OnSwapWeaponMontageEnded(UAnimMontage* Montage, bool bInterrupted);

public:
    ABaseWeapon* GetNextWeaponWithAmmo() const;
    void AutoSwapToAvailableWeapon();

    // -----------------------------------------------------------------------------
    // Input Mapping & Actions
    // -----------------------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

protected:
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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Input")
    TObjectPtr<UInputAction> AbilityAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Input")
    TObjectPtr<UInputAction> SwapWeaponAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Input")
    TObjectPtr<UInputAction> InteractAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Input")
    TObjectPtr<UInputAction> GamepadXAction;

    UPROPERTY(EditDefaultsOnly, Category = "Hama | Input")
    TObjectPtr<UInputAction> MeleeAction;

    // -----------------------------------------------------------------------------
    // UI & HUD (MainWidgetRef Removed)
    // -----------------------------------------------------------------------------

protected:
    virtual void OnPlayerStateChanged(APlayerState* NewPlayerState, APlayerState* OldPlayerState) override;

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hama|Animations")
    UAnimMontage* DiveMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hama|Animations")
    UAnimMontage* DrinkPerkMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hama|Animations")
    UAnimMontage* SwapWeaponMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hama|Animations")
    UAnimMontage* RevivingAnimationMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Hama | Melee")
    UAnimMontage* MeleeAttackMontage;

public:
    // -----------------------------------------------------------------------------
    // Blueprint Events
    // -----------------------------------------------------------------------------

    UFUNCTION(BlueprintImplementableEvent, Category = "Hama|Events")
    void OnAim(bool InAiming);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void Landed(const FHitResult& Hit) override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void OnRep_Controller() override;
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, class AActor* DamageCauser) override;

    UFUNCTION()
    void OnRep_CurrentWeapon();

protected:
    UPROPERTY(BlueprintReadOnly, Category = "Hama|Camera")
    bool bIsInFirstPerson = false;

    UPROPERTY(BlueprintReadOnly, Category = "Hama|Camera")
    bool bIsInRightShoulderView = false;

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
    void CrouchActionPressed(const FInputActionInstance& Instance);
    void CrouchActionReleased(const FInputActionInstance& Instance);
    void StartSlideRoutine();
    void StopSlideRoutine();
    void StartDiving();
    void StopDiving();
    void SwitchCameraPressed(const FInputActionInstance& Instance);
    void SwitchCameraReleased();
    void SprintActionPressed();
    void OnSprintStopped();
    void StartCrossHairTimer();
    void CrossHairTrace();
    void OnCrossHairTraceCompleted(const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum);
    void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);
    void OnDiveMontageEnded(UAnimMontage* Montage, bool bInterrupted);
    void CreateDefaultWeapon();
    void AttachWeaponToMesh(ABaseWeapon* WeaponToAttach);
    void ReloadActionPressed();
    void AimPressedSitck();
    void AbilityActionPressed();
    void MeleeActionPressed();
    void InteractActionReleased();

protected:
    static const float CrossHairTimer;

    bool bIsHoldedTrigger = false;
    bool bIsAimButtonHold = false;
    bool bIsAimSnapping = false;
    bool bHasPerformedDive = false;
    FRotator TargetSnapRotation;

    UPROPERTY()
    TObjectPtr<AZombie> SnapTarget;

    FName SnapSocketName = FName("spine_03");

    float SnapInterpSpeed = 45.f;
    float SnapStopThreshold = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hama|Targeting")
    float StickySlowdownMultiplier = 0.5f;

public:
    bool bIsFireButtonHold = false;
    void ResetValuesAfterSprint();

private:
    FTimerHandle CrossHairTimerHandle;
    bool bLastCrossHairState = false;

public:
    void StopSprint();
    FORCEINLINE bool IsSprinting() const { return HamaComponent && HamaComponent->IsSprinting(); }
    FORCEINLINE bool IsAiming() const { return HamaComponent && HamaComponent->IsAiming(); }
    FORCEINLINE ABaseWeapon* GetCurrentWeapon() const { return CurrentWeapon; }
    FORCEINLINE bool GetDeathMachine() const { return bIsDeathMachineActive; }
    bool IsGhost() const { return HamaAbilityComponent && HamaAbilityComponent->GetGhost(); }
    const TArray<FName>& GetOwnedPerks() const { return OwnedPerks; }
    bool DrinkingPerkTimer() const { return GetWorldTimerManager().IsTimerActive(PerkDrinkTimerHandle); }
    bool GetDoubleTap() { return bHasDoubleTap; }
    bool HasDeadshot() const { return bHasDeadshot; }
    bool IsDowned() const { return HamaComponent && HamaComponent->IsDowned(); }
    bool IsDrinkingPerk() const { return CurrentSpawnedBottle != nullptr; }
    bool HasQuickRevive() const { return bHasQuickRevive; }
    bool IsMovingForward() const;
    bool IsSliding() const { return HamaComponent->IsSlide(); }
    bool IsDiving() const { return HamaComponent->IsDiving(); }

protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Input|Sensitivity")
    float NormalSensitivity = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Input|Sensitivity")
    float AimingSensitivity = 0.5f;

public:
    UPROPERTY(EditDefaultsOnly, Category = "Hama|Roles Visuals")
    TMap<EHamaAbilityType, FRoleVisualData> RoleVisuals;

    void ApplyRoleVisuals(EHamaAbilityType NewRole);

public:
    void RefillAllWeapons();

    UFUNCTION(BlueprintCallable, Category = "Weapon System")
    void RemoveCurrentWeapon();

protected:
    void HandleAmmoChanged(int32 CurrentAmmo, int32 ReserveAmmo);

    // -----------------------------------------------------------------------------
    // AAA Dynamic Perk System
    // -----------------------------------------------------------------------------
protected:
    UPROPERTY(ReplicatedUsing = OnRep_OwnedPerks, BlueprintReadOnly, Category = "Hama|Perks")
    TArray<FName> OwnedPerks;

    UFUNCTION()
    void OnRep_OwnedPerks();

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hama|Perks")
    bool bHasFastHands = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hama|Perks")
    bool bHasDoubleTap = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hama|Perks")
    bool bHasDeadshot = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hama|Perks")
    bool bHasMuleKick = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hama|Perks")
    bool bHasQuickRevive = false;

    FName PendingPerkID;

    UPROPERTY()
    class AStaticMeshActor* CurrentSpawnedBottle;

public:
    UPROPERTY(Replicated)
    bool bIsDeathMachineActive = false;

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayDrinkPerkAnimation(ABasePerk* TargetPerk);

    UFUNCTION()
    void OnDrinkPerkAnimationCompleteFromMontage(UAnimMontage* Montage, bool bInterrupted);

    void AddPerkByID(FName PerkID);
    void Server_StartPerkDrink(ABasePerk* TargetPerk);

    bool HasPerkID(FName PerkIDToCheck) const { return OwnedPerks.Contains(PerkIDToCheck); }
    bool HasFastHands() const { return bHasFastHands; }
    void HandleDeath();

protected:
    FTimerHandle PerkDrinkTimerHandle;
    void GivePendingPerk();

public:
    UFUNCTION(Client, Unreliable)
    void Client_ShowDamageIndicator(FVector DamageOrigin);

    UFUNCTION(BlueprintImplementableEvent, Category = "UI")
    void OnDamageIndicatorUpdate(float Angle);

protected:
    void CheckForInteractables();

    void OnInteractTraceCompleted(const FTraceHandle& Handle, FTraceDatum& Datum);

    UPROPERTY()
    FTimerHandle InteractTimerHandle;

    class IInteractInterface* FocusedInteractable;

    void InteractActionPressed();
    void GamepadXActionPressed(const FInputActionInstance& Instance);
    void GamepadXActionReleased();

    bool bIsxButtonHolded = false;

    UFUNCTION(Server, Reliable)
    void Server_Interact(AActor* InteractTarget);

public:
    UPROPERTY(Replicated, BlueprintReadOnly)
    bool bIsDead = false;

protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Setting")
    float SetIntractDistance = 100.f;

    UFUNCTION(Server, Reliable)
    void Server_ExecuteMelee();

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_ValidateMeleeHit(AActor* HitActor, FVector_NetQuantize HitLocation);

    UPROPERTY(EditDefaultsOnly, Category = "Hama | Melee")
    float MeleeDamage = 150.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Hama | Melee")
    float MeleeRange = 150.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Hama | Melee")
    float MeleeRadius = 40.0f;

    bool IsMeleeing() const;

public:
    void PerformMeleeHitDetection();

public:
    UFUNCTION(Server, Reliable)
    void Server_BeginRevive(AHama* DownedPlayer);

    UFUNCTION(Server, Reliable)
    void Server_CancelRevive();

    // ⚡ گۆڕدرا: لادانی Parameter بۆ ڕێگری لە Memory Crash
    UFUNCTION()
    void Server_CompleteRevive();

    void Server_CheckReviveConditions();
    void ClearAllReviveTimers();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hama | Revive")
    float DefaultReviveTime = 5.0f;

private:
    FTimerHandle ReviveTimerHandle;
    FTimerHandle ReviveCheckTimerHandle;

    UPROPERTY()
    TWeakObjectPtr<AHama> CurrentReviveTarget;

    bool bIsCurrentlyReviving = false;

    virtual bool CanInteract(AHama* InteractingPlayer) override;
    virtual FString GetInteractMessage() override;
    virtual void Interact(AHama* InteractingPlayer) override;

    UFUNCTION()
    void OnInteractSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnInteractSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    int32 NearbyInteractablesCount = 0;

    UFUNCTION(Client, Reliable)
    void Client_OnStaminUpAcquired(float NewMaxStamina);

    UFUNCTION(Client, Reliable)
    void Client_OnPlayerDowned();

 public:
     UFUNCTION(BlueprintCallable, Category = "Weapons")
     TArray<TSubclassOf<class ABaseWeapon>> GetOwnedWeaponClasses() const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapons|Inventory")
    TArray<TObjectPtr<ABaseWeapon>> EquippedWeapons;
};