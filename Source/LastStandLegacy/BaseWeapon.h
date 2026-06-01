// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h" // پێویستە بۆ Data Table
#include "BaseWeapon.generated.h"

#define ECC_Bullet ECC_GameTraceChannel1

class AHama;
class UHamaComponent;
class USkeletalMeshComponent;
class UAnimMontage;

/** دیاریکردنی شێوازی تەقەکردنی چەکەکە */
UENUM(BlueprintType)
enum class EWeaponFireMode : uint8
{
	Single     UMETA(DisplayName = "Single Shot"),
	Burst      UMETA(DisplayName = "Burst"),
	Automatic  UMETA(DisplayName = "Full Automatic")
};

/** پێکهاتەی زانیارییەکانی چەک بۆ ناو Data Table */
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
	float FireRate = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Core")
	float MaxRange = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Core")
	int32 MaxZombiePenetration = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Core")
	int32 BurstShotCount = 3;

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
};

UCLASS()
class LASTSTANDLEGACY_API ABaseWeapon : public AActor
{
	GENERATED_BODY()

public:
	ABaseWeapon();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Mesh")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	// -----------------------------------------------------------------------------
	// Data Table Setup
	// -----------------------------------------------------------------------------
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Data")
	UDataTable* WeaponDataTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Data")
	FName WeaponRowName;

	// فەنکشن بۆ خوێندنەوەی زانیارییەکان لە داتاتەیبڵەوە
	void InitializeWeaponData();

protected:
	// زانیارییە ناوخۆییەکانی ئەم چەکە کاتێک لە داتابەیسەکەوە دەیخوێنێتەوە
	FWeaponData CurrentWeaponData;
	// ئەو گۆڕاوانەی کە پێویستە بەردەوام نوێ ببنەوە یان ڕیپڵیکەیت بن
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon|LiveStats")
	int32 CurrentAmmo;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon|LiveStats")
	int32 MaxAmmoInClip;

	UPROPERTY(Transient, ReplicatedUsing = OnRep_BurstCounter)
	uint8 BurstCounter = 0;

	FTimerHandle FireTimerHandle;
	FTimerHandle ReloadTimerHandle;

	float LastFireTime;
	int32 CurrentBurstShotsLeft = 0;

public:

	UPROPERTY(ReplicatedUsing = OnRep_Reload, BlueprintReadOnly, Category = "Weapon|LiveStats")
	bool bIsReloading = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon|LiveStats")
	int32 ReserveAmmo;

public:
	void StartFire();
	void StopFire();
	void HandleFireLocal();
	float CalculateBulletSpread();

	UFUNCTION(Server, Reliable)
	void ServerHandleFire(FVector StartLocation, FVector EndLocation);

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

	UPROPERTY()
	AHama* OwnerCharacter;

	UPROPERTY()
	UHamaComponent* HamaComponent;

	UPROPERTY()
	APlayerController* OwnerController;


public:
	FORCEINLINE float GetWeaponMaxRange() const { return CurrentWeaponData.MaxRange; }
};