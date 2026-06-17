#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "Camera/CameraShakeBase.h" 
#include "BaseWeapon.generated.h"

#define ECC_Bullet ECC_GameTraceChannel1

class AHama;
class UHamaComponent;
class USkeletalMeshComponent;
class UAnimMontage;
class UAnimSequence;
class UForceFeedbackEffect;
class UCurveFloat;

UENUM(BlueprintType)
enum class EWeaponFireMode : uint8
{
    Single    UMETA(DisplayName = "Single Shot"),
    Burst     UMETA(DisplayName = "Burst"),
    Automatic UMETA(DisplayName = "Full Automatic")
};

USTRUCT(BlueprintType)
struct FWeaponData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Core")
    EWeaponFireMode FireMode = EWeaponFireMode::Automatic;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Core")
    float Damage = 25.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Core")
    float HeadshotMultiplier = 2.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Core")
    float LegDamageMultiplier = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Core")
    float FireRate = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Core")
    float MaxRange = 5000.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Core")
    int32 MaxZombiePenetration = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Core")
    int32 BurstShotCount = 3;

    // --- سیستەمی نوێی AAA Recoil ---
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|AAA_Recoil")
    TSubclassOf<UCameraShakeBase> FireCameraShake;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|AAA_Recoil")
    UCurveFloat* RecoilPitchCurve;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|AAA_Recoil")
    float RecoilRandomness = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|AAA_Recoil")
    float AimRecoilMultiplier = 0.55f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Spread")
    float BulletSpread = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Spread")
    float CrouchSpreadMultiplier = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Spread")
    float AirSpreadMultiplier = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Ammo")
    int32 MaxAmmoInClip = 30;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Ammo")
    int32 MaxReserveAmmo = 220;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Ammo")
    float DefaultReloadTime = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visuals")
    TSoftObjectPtr<USkeletalMesh> WeaponMeshAsset;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visuals")
    UAnimMontage* ReloadMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Visuals")
    FName MuzzleLocationName = FName("Muzzle");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Animations")
    UAnimSequence* AimMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Animations")
    UAnimSequence* WeaponIdle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Animations")
    UAnimSequence* WeaponSprint;
};

UCLASS()
class LASTSTANDLEGACY_API ABaseWeapon : public AActor
{
    GENERATED_BODY()

public:
    ABaseWeapon();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Mesh")
    TObjectPtr<USkeletalMeshComponent> WeaponMesh;

    void EquipWeapon(AHama* NewOwnerCharacter);

    UFUNCTION(BlueprintCallable, Server, Reliable)
    void ServerUpgradeWeapon_PackAPunch();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void OnRep_Owner() override;
    void UpdateCachedReferences();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    float CalculateDamageBySurface(const FHitResult& Hit);

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Data")
    UDataTable* WeaponDataTable;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Data")
    FName WeaponRowName;

    void InitializeWeaponData();

protected:
    FWeaponData CurrentWeaponData;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon|LiveStats")
    int32 CurrentAmmo;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon|LiveStats")
    int32 MaxAmmoInClip;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon|LiveStats")
    float Damage;

    UPROPERTY(Transient, ReplicatedUsing = OnRep_BurstCounter)
    uint8 BurstCounter = 0;

    FTimerHandle FireTimerHandle;
    FTimerHandle ReloadTimerHandle;
    FTimerHandle ServerFireTimerHandle;

    int32 CurrentBurstShotsLeft = 0;
    int32 ServerBurstShotsLeft = 0; // لۆجیکی جیاکراوە بۆ سێرڤەر بۆ پاراستن لە هاک
    float NextAllowedFireTime;

public:
    UPROPERTY(ReplicatedUsing = OnRep_Reload, BlueprintReadOnly, Category = "Weapon|LiveStats")
    bool bIsReloading = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon|LiveStats")
    int32 ReserveAmmo;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Effects")
    UForceFeedbackEffect* ShootForceFeedback;

public:
    void StartFire();
    void StopFire();
    void HandleFireLocal();
    float CalculateBulletSpread();

    UFUNCTION(Server, Reliable)
    void Server_StartFire();

    UFUNCTION(Server, Reliable)
    void Server_StopFire();

    void Server_FireRoutine();

    UFUNCTION()
    void OnRep_BurstCounter();

    void Reload();

    UFUNCTION(Server, Reliable)
    void ServerReload(float InReloadTime);

    void CancelReload();

    UFUNCTION(Server, Reliable)
    void Server_CancelReload();

    UFUNCTION()
    void OnRep_Reload();

protected:
    void Local_ReloadComplete();
    void Server_ReloadComplete();
    void PlayWeaponEffects();

    void PlayLocalHitEffects(const FHitResult& LocalHit);

    void ApplyRecoilAndCameraShake();
    void ResetRecoil();
    int32 ShotsFiredInBurst = 0;

    FRotator TargetRecoilOffset = FRotator::ZeroRotator;
    FRotator CurrentRecoilOffset = FRotator::ZeroRotator;

    UPROPERTY()
    TObjectPtr<AHama> OwnerCharacter;

    UPROPERTY()
    UHamaComponent* HamaComponent;

    UPROPERTY()
    TObjectPtr<APlayerController> OwnerController;

public:
    FORCEINLINE float GetWeaponMaxRange() const { return CurrentWeaponData.MaxRange; }

    UFUNCTION(BlueprintCallable)
    FORCEINLINE UAnimSequence* GetAimMontage() const { return CurrentWeaponData.AimMontage; }

    UFUNCTION(BlueprintCallable)
    FORCEINLINE UAnimSequence* GetWeaponIdle() const { return CurrentWeaponData.WeaponIdle; }

    UFUNCTION(BlueprintCallable)
    FORCEINLINE UAnimSequence* GetWeaponSprint() const { return CurrentWeaponData.WeaponSprint; }
};