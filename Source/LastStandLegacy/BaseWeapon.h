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
	float FireRate = 0.1f; // کاتی نێوان فیشەکەکان بە چرکە (بۆ نموونە 0.1 واتە 10 فیشەک لە چرکەیەکدا)

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseWeapon|Stats")
	float MaxRange = 5000.f; // مەودای فیشەکەکە بە سانتیمەتر (50 مەتر)

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stat|Spread", meta = (AllowPrivateAccess = "true"))
	float BulletSpread = 2.0f;

	// ڕێژەی کەمبوونەوەی بڵاوبوونەوە کاتێک یاریزانەکە دادەنیشێت (0.5 یعنی دەبێتە نیوە)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stat|Spread", meta = (AllowPrivateAccess = "true"))
	float CrouchSpreadMultiplier = 0.5f;

	// ڕێژەی زیادبوونی بڵاوبوونەوە کاتێک یاریزانەکە لە ئاسماندایە (2.0 یعنی دوو هێندە دەبێت)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stat|Spread", meta = (AllowPrivateAccess = "true"))
	float AirSpreadMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseWeapon|Stats")
	int32 MaxZombiePenetration = 1;

	UPROPERTY(ReplicatedUsing = OnRep_Reload, EditAnywhere, BlueprintReadOnly, Category = "Weapon Stat|Ammo")
	bool bIsReloading = false;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Weapon Stat|Ammo")
	int32 CurrentAmmo = 30;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Weapon Stat|Ammo")
	int32 MaxAmmoInClip = 30;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Weapon Stat|Ammo")
	int32 ReserveAmmo = 220;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Stat|Ammo")
	FName MuzzleLocationName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly ,Category = "Weapon Stat|Animation")
	UAnimMontage* ReloadMontage;
	
public:
	void StartFire();
	void StopFire();
	void HandleFireLocal();
	float CalculateBulletSpread();

	UFUNCTION(Server, Reliable)
	void ServerHandleFire(FVector StartLocation, FVector EndLocation);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayFireEffects();

	void Reload();

	UFUNCTION(Server, Reliable)
	void ServerReload();

	UFUNCTION()
	void OnRep_Reload();

private:
	FTimerHandle FireTimerHandle;
	void PlayWeaponEffects();
	UPROPERTY()
	AHama* OwnerCharacter;

	UPROPERTY()
	UHamaComponent* HamaComponent;

	UPROPERTY()
	APlayerController* OwnerController;
};