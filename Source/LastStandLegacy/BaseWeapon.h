// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BaseWeapon.generated.h"

#define ECC_Zombie ECC_GameTraceChannel1

class AHama;
class UHamaComponent;
class USkeletalMeshComponent;
class UAnimMontage;

// دیاریکردنی شێوازی تەقەکردنی چەکەکە
UENUM(BlueprintType)
enum class EWeaponFireMode : uint8
{
	Single     UMETA(DisplayName = "Single Shot"),
	Burst      UMETA(DisplayName = "Burst"),
	Automatic  UMETA(DisplayName = "Full Automatic")
};

UCLASS()
class LASTSTANDLEGACY_API ABaseWeapon : public AActor
{
	GENERATED_BODY()

public:
	ABaseWeapon();

	UPROPERTY(EditDefaultsOnly, Category = "BaseWeapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	virtual void Tick(float DeltaTime) override;

public:
	// -----------------------------------------------------------------------------
	// Weapon Core Stats (زانیارییە سەرەکییەکانی چەک)
	// -----------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseWeapon|Stats")
	EWeaponFireMode FireMode = EWeaponFireMode::Automatic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseWeapon|Stats")
	float Damage = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseWeapon|Stats")
	float HeadshotMultiplier = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseWeapon|Stats")
	float FireRate = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseWeapon|Stats")
	float MaxRange = 5000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stat|Spread", meta = (AllowPrivateAccess = "true"))
	float BulletSpread = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stat|Spread", meta = (AllowPrivateAccess = "true"))
	float CrouchSpreadMultiplier = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stat|Spread", meta = (AllowPrivateAccess = "true"))
	float AirSpreadMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseWeapon|Stats")
	int32 MaxZombiePenetration = 1;

	// گۆڕاوی ڕیلۆد 
	UPROPERTY(ReplicatedUsing = OnRep_Reload, BlueprintReadOnly, Category = "Weapon Stat|Ammo")
	bool bIsReloading = false;

	// بژمێری تەقەکردن لەبری Multicast (بۆ یاریزانەکانی تر بۆ ئەوەی بزانن چەکەکە تەقەی کردووە)
	UPROPERTY(Transient, ReplicatedUsing = OnRep_BurstCounter)
	uint8 BurstCounter = 0;

public:
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Weapon Stat|Ammo")
	int32 CurrentAmmo = 30;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Weapon Stat|Ammo")
	int32 MaxAmmoInClip = 30;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Weapon Stat|Ammo")
	int32 ReserveAmmo = 220;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Stat|Ammo")
	FName MuzzleLocationName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Stat|Animation")
	UAnimMontage* ReloadMontage;

public:
	void StartFire();
	void StopFire();
	void HandleFireLocal();
	float CalculateBulletSpread();

	UFUNCTION(Server, Reliable)
	void ServerHandleFire(FVector StartLocation, FVector EndLocation);

	// پێویستت بە Multicast نامێنێت بۆ فیشەکەکەکان، گۆڕدرا بۆ ئەمە
	UFUNCTION()
	void OnRep_BurstCounter();

	void Reload();

	UFUNCTION(Server, Reliable)
	void ServerReload();

	// BlueprintCallable کراوە بۆ ئەوەی لە ناو Anim Notify ی مۆنتاژەکە بتوانێت بانگ بکرێت
	UFUNCTION(BlueprintCallable, Category = "Weapon Stat|Ammo")
	void ReloadFinish();

	// ڕەسەنی سەلماندنی خێراکەری سێرڤەر بۆ ڕیلۆد
	UFUNCTION(Server, Reliable)
	void Server_FinishReloadValidation();

	void CancelReload();

	UFUNCTION(Server, Reliable)
	void Server_CancelReload();

	UFUNCTION()
	void OnRep_Reload();

private:
	FTimerHandle FireTimerHandle;
	float LastFireTime;

	void PlayWeaponEffects();

	UPROPERTY()
	AHama* OwnerCharacter;

	UPROPERTY()
	UHamaComponent* HamaComponent;

	UPROPERTY()
	APlayerController* OwnerController;
};