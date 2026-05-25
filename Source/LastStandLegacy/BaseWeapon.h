// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BaseWeapon.generated.h"

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
	float BaseDamage = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseWeapon|Stats")
	float HeadshotMultiplier = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseWeapon|Stats")
	float FireRate = 0.1f; // کاتی نێوان فیشەکەکان بە چرکە (بۆ نموونە 0.1 واتە 10 فیشەک لە چرکەیەکدا)

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseWeapon|Stats")
	float MaxRange = 5000.f; // مەودای فیشەکەکە بە سانتیمەتر (50 مەتر)

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseWeapon|Stats")
	float ReloadTime = 2.5f; // کاتی پێویست بۆ ڕیلۆد بە چرکە

	// -----------------------------------------------------------------------------
	// Ammo System (سیستەمی فیشەک)
	// -----------------------------------------------------------------------------
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "BaseWeapon|Ammo")
	int32 CurrentAmmoInMag; // ژمارەی فیشەکی ناو مەخزەنەکە ئێستا

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseWeapon|Ammo")
	int32 MaxMagSize = 30; // بەرزترین ڕێژەی فیشەک لە یەک مەخزەندا

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "BaseWeapon|Ammo")
	int32 CurrentReserveAmmo; // فیشەکی یەدەگ کە پێیەتی

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseWeapon|Ammo")
	int32 MaxReserveAmmo = 210; // بەرزترین ڕێژەی فیشەکی یەدەگ

	// -----------------------------------------------------------------------------
	// Zombie Penetration & AoE (بۆ ئەوەی بزانین چەکەکە چەند زۆمبی دەکاتە ئامانج)
	// -----------------------------------------------------------------------------

	/* بۆ چەکی ئاسایی (وەک سنایپەر یان ڕاڕەوی فیشەک):
	   فیشەکەکە بە ناو چەند زۆمبیدا تێپەڕ دەبێت؟ (1 واتە تەنها یەک زۆمبی، 3 واتە بە ناو 3 زۆمبیدا دەڕوات و زیانیان پێدەگەیەنێت) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseWeapon|ZombieLogic")
	int32 MaxZombiePenetration = 1;

	/* بۆ چەکی تەقینەوەیی (وەک RPG یان لۆنچەر):
	   ئایا چەکەکە زیانی بەکۆمەڵ (Splash/AoE Damage) دەگەیەنێت؟ */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseWeapon|ZombieLogic")
	bool bIsExplosive = false;

	/* نیوەتیرەی تەقینەوەکە (Radius) بە سانتیمەتر، بۆ ئەوەی بزانین لە دەوروبەری شوێنی پێکانی فیشەکەکە چەند زۆمبی هەیە زیانیان پێبگات */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseWeapon|ZombieLogic", meta = (EditCondition = "bIsExplosive"))
	float ExplosionRadius = 300.f;

	// -----------------------------------------------------------------------------
	// Handling & Recoil (کۆنتڕۆڵ و لەرزینی چەک)
	// -----------------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseWeapon|Handling")
	float VerticalRecoil = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseWeapon|Handling")
	float HorizontalRecoil = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseWeapon|Handling")
	float WeaponSpread = 0.02f; // بڵاوبوونەوەی فیشەک (کەم بێت نیشانەکەی ڕاستترە)

	// -----------------------------------------------------------------------------
	// Visual & Audio Effects (کاریگەرییە بینراو و بیستراوەکان)
	// -----------------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseWeapon|Effects")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseWeapon|Effects")
	TObjectPtr<UParticleSystem> MuzzleFlash;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BaseWeapon|Effects")
	TObjectPtr<UParticleSystem> ImpactEffect; // کاتێک فیشەکەکە لە زۆمبی یان دیوار دەدات
};