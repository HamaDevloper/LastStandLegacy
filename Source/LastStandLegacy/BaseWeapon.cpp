// Fill out your copyright notice in the Description page of Project Settings.

#include "BaseWeapon.h"
#include "Hama.h"
#include "HamaComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ABaseWeapon::ABaseWeapon()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	RootComponent = WeaponMesh;
}

void ABaseWeapon::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<AHama>(GetOwner());
	if (OwnerCharacter)
	{
		HamaComponent = OwnerCharacter->FindComponentByClass<UHamaComponent>();
		OwnerController = Cast<APlayerController>(OwnerCharacter->GetController());
	}
}

void ABaseWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ABaseWeapon, bIsReloading, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ABaseWeapon, CurrentAmmo, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ABaseWeapon, MaxAmmoInClip, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ABaseWeapon, ReserveAmmo, COND_OwnerOnly);
}

void ABaseWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ABaseWeapon::StartFire()
{
	if (bIsReloading) return;

	if (CurrentAmmo <= 0) Reload();

	if (FireMode == EWeaponFireMode::Single)
	{
		HandleFireLocal();
	}
	else
	{
		HandleFireLocal();
		GetWorldTimerManager().SetTimer(FireTimerHandle, this, &ABaseWeapon::HandleFireLocal, FireRate, true);
	}
}

void ABaseWeapon::StopFire()
{
	GetWorldTimerManager().ClearTimer(FireTimerHandle);
}

float ABaseWeapon::CalculateBulletSpread()
{
	// ئەگەر نیشانەی گرتبووەوە، با بە تەواوی دقیق بێت و بڵاوبوونەوە نەبێت
	if (HamaComponent && HamaComponent->bIsAiming)
	{
		return 0.f;
	}

	float CurrentSpread = BulletSpread;

	// دڵنیابوونەوە لەوەی کاراکتەر و پێکهاتەی جوڵەکەی بوونیان هەیە
	if (OwnerCharacter && OwnerCharacter->GetCharacterMovement())
	{
		UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement();

		if (MoveComp->IsCrouching())
		{
			// کاتێک دانیشتووە بڵاوبوونەوەکە کەم دەکەینەوە (بۆ نموونە دەیکەین بە نیوە)
			CurrentSpread *= CrouchSpreadMultiplier;
		}
		else if (MoveComp->IsFalling())
		{
			// کاتێک لە ئاسمانە یان باز دەدات، بڵاوبوونەوەکە دوو هێندە زیاد دەکەین
			CurrentSpread *= AirSpreadMultiplier;
		}
	}

	return CurrentSpread;
}

void ABaseWeapon::HandleFireLocal()
{
	if (!OwnerCharacter || !OwnerCharacter->IsLocallyControlled()) return;
	if (!OwnerController)
	{
		OwnerController = Cast<APlayerController>(OwnerCharacter->GetController());
		if (!OwnerController) return;
	}

	FVector Start;
	FRotator Rotation;
	OwnerController->GetPlayerViewPoint(Start, Rotation);

	float Spread = CalculateBulletSpread();
	float SpreadInRadians = FMath::DegreesToRadians(Spread);

	FVector SpreadDirection = FMath::VRandCone(Rotation.Vector(), SpreadInRadians);

	FVector FinalEnd = Start + (SpreadDirection * MaxRange);

	PlayWeaponEffects();

	ServerHandleFire(Start, FinalEnd);
}

void ABaseWeapon::ServerHandleFire_Implementation(FVector StartLocation, FVector EndLocation)
{
	AController* DamageInstigator = OwnerCharacter ? OwnerCharacter->GetController() : nullptr;

	// گۆڕاوێک بۆ دیاریکردنی خاڵی کۆتایی هێڵە سوورەکە (ئەگەر بەر هیچ نەکەوێت، هەر هەمان EndLocationە)
	FVector LineVisualEnd = EndLocation;

	CurrentAmmo--;

	if (MaxZombiePenetration <= 1)
	{
		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);
		Params.AddIgnoredActor(GetOwner());

		// لێرەدا Line Traceەکە بەو ئاراستە بڵاوبووەوەیە دەچێت کە لۆکاڵی حیسابکراوە
		bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, StartLocation, EndLocation, ECC_Zombie, Params);
		if (bHit)
		{
			UGameplayStatics::ApplyPointDamage(
				Hit.GetActor(),
				Damage,
				Hit.ImpactNormal,
				Hit,
				DamageInstigator,
				this,
				UDamageType::StaticClass()
			);

			// ئەگەر بەر زۆمبی یان دیوار کەوت، با هێڵە سوورەکە ڕێک لەسەر خاڵی پێکانەکە بوەستێت
			LineVisualEnd = Hit.ImpactPoint;
		}
	}
	else
	{
		TArray<FHitResult> Hits;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);
		Params.AddIgnoredActor(GetOwner());

		GetWorld()->LineTraceMultiByChannel(Hits, StartLocation, EndLocation, ECC_Zombie, Params);

		int32 PenetrationCount = 0;
		TSet<AActor*> HitActors;

		for (const FHitResult& Hit : Hits)
		{
			if (Hit.GetActor() && !HitActors.Contains(Hit.GetActor()))
			{
				UGameplayStatics::ApplyPointDamage(
					Hit.GetActor(),
					Damage,
					Hit.ImpactNormal,
					Hit,
					DamageInstigator,
					this,
					UDamageType::StaticClass()
				);

				HitActors.Add(Hit.GetActor());
				PenetrationCount++;

				// بۆ حاڵەتی پێپەڕبوون، با هێڵەکە تا کۆتا زۆمبی بڕوات کە فیشەکەکەی پێوە بووە
				LineVisualEnd = Hit.ImpactPoint;

				if (PenetrationCount >= MaxZombiePenetration) break;
			}
		}
	}
	FVector MuzzleLocation = WeaponMesh->GetSocketLocation(MuzzleLocationName);
	DrawDebugLine(GetWorld(), MuzzleLocation, LineVisualEnd, FColor::Red, false, 2.f, 0.f, 3.f);

	MulticastPlayFireEffects();

	if (CurrentAmmo <= 0) Reload();
}

void ABaseWeapon::MulticastPlayFireEffects_Implementation()
{
	// ئەگەر خۆت تەقەت کردووە پێویست ناکات دووبارە لێبدرێتەوە، چونکە لۆکاڵی لێدراوە
	if (OwnerCharacter && OwnerCharacter->IsLocallyControlled()) return;
	PlayWeaponEffects();
}

void ABaseWeapon::PlayWeaponEffects()
{
}

void AHama::Reload()
{
	if (ReserveAmmo <= 0 || bIsReloading) return;
	if (ReloadMontage)
	{
		OwnerCharacter->PlayAnimMontage(ReloadMontage);
		
	}
}

void ABaseWeapon::ServerReload_Implementation()
{
}

void ABaseWeapon::OnRep_Reload()
{
	if (OwnerCharacter && OwnerCharacter->IsLocallyControlled()) return;
}