// Fill out your copyright notice in the Description page of Project Settings.

#include "BaseWeapon.h"
#include "Hama.h"
#include "HamaComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"

ABaseWeapon::ABaseWeapon()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	RootComponent = WeaponMesh;

	LastFireTime = 0.f;
}

void ABaseWeapon::BeginPlay()
{
	Super::BeginPlay();

	// یەکەم هەنگاو: هێنانی زانیاری چەکەکە لە داتاتەیبڵەوە
	InitializeWeaponData();

	OwnerCharacter = Cast<AHama>(GetOwner());
	if (OwnerCharacter)
	{
		HamaComponent = OwnerCharacter->FindComponentByClass<UHamaComponent>();
		OwnerController = Cast<APlayerController>(OwnerCharacter->GetController());
	}
}

void ABaseWeapon::InitializeWeaponData()
{
	if (WeaponDataTable && !WeaponRowName.IsNone())
	{
		FWeaponData* Row = WeaponDataTable->FindRow<FWeaponData>(WeaponRowName, TEXT("Weapon Context"));
		if (Row)
		{
			CurrentWeaponData = *Row;

			// دانانی بەها سەرەتاییەکان لە داتاتەیبڵەکەوە
			MaxAmmoInClip = CurrentWeaponData.MaxAmmoInClip;
			CurrentAmmo = MaxAmmoInClip;
			ReserveAmmo = CurrentWeaponData.MaxReserveAmmo;

			// چارەسەری هەڵەکە: لێرەدا بە Cast فۆرماتەکەی بۆ ڕێکدەخەینەوە
			if (WeaponMesh && !CurrentWeaponData.WeaponMeshAsset.IsNull())
			{
				USkeletalMesh* LoadedMesh = Cast<USkeletalMesh>(CurrentWeaponData.WeaponMeshAsset.LoadSynchronous());
				if (LoadedMesh)
				{
					WeaponMesh->SetSkeletalMesh(LoadedMesh);
				}
			}
		}
	}
}

void ABaseWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ABaseWeapon, bIsReloading, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(ABaseWeapon, CurrentAmmo, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ABaseWeapon, ReserveAmmo, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ABaseWeapon, MaxAmmoInClip, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ABaseWeapon, BurstCounter, COND_SkipOwner);
}

void ABaseWeapon::StartFire()
{
	if (CurrentAmmo <= 0 && ReserveAmmo <= 0) return;

	if (bIsReloading)
	{
		if (CurrentAmmo > 0) CancelReload();
		else return;
	}

	float CurrentTime = GetWorld()->GetTimeSeconds();

	if (CurrentWeaponData.FireMode == EWeaponFireMode::Burst)
	{
		CurrentBurstShotsLeft = CurrentWeaponData.BurstShotCount;
	}

	if (CurrentTime - LastFireTime >= CurrentWeaponData.FireRate)
	{
		HandleFireLocal();

		if (CurrentWeaponData.FireMode == EWeaponFireMode::Automatic || CurrentWeaponData.FireMode == EWeaponFireMode::Burst)
		{
			GetWorldTimerManager().SetTimer(FireTimerHandle, this, &ABaseWeapon::HandleFireLocal, CurrentWeaponData.FireRate, true);
		}
	}
}

void ABaseWeapon::StopFire()
{
	if (CurrentWeaponData.FireMode != EWeaponFireMode::Burst || CurrentBurstShotsLeft <= 0)
	{
		GetWorldTimerManager().ClearTimer(FireTimerHandle);

		if (HamaComponent && HamaComponent->IsFiring())
		{
			HamaComponent->SetFiring(false);
		}
	}
}

float ABaseWeapon::CalculateBulletSpread()
{
	if (HamaComponent && HamaComponent->bIsAiming)
	{
		return 0.f;
	}

	float CurrentSpread = CurrentWeaponData.BulletSpread;

	if (OwnerCharacter && OwnerCharacter->GetCharacterMovement())
	{
		UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement();

		if (MoveComp->IsCrouching())
		{
			CurrentSpread *= CurrentWeaponData.CrouchSpreadMultiplier;
		}
		else if (MoveComp->IsFalling())
		{
			CurrentSpread *= CurrentWeaponData.AirSpreadMultiplier;
		}
	}

	return CurrentSpread;
}

void ABaseWeapon::HandleFireLocal()
{
	if (!OwnerCharacter || !OwnerCharacter->IsLocallyControlled()) return;

	if (CurrentAmmo <= 0)
	{
		StopFire();
		Reload();
		return;
	}

	if (CurrentWeaponData.FireMode == EWeaponFireMode::Burst)
	{
		if (CurrentBurstShotsLeft <= 0)
		{
			StopFire();
			return;
		}
		CurrentBurstShotsLeft--;
	}

	if (!OwnerController)
	{
		OwnerController = Cast<APlayerController>(OwnerCharacter->GetController());
		if (!OwnerController) return;
	}

	CurrentAmmo--;
	LastFireTime = GetWorld()->GetTimeSeconds();

	if (HamaComponent->bIsSprinting) HamaComponent->StopSprinting();
	if (!HamaComponent->IsFiring()) HamaComponent->SetFiring(true);

	FVector Start;
	FRotator Rotation;
	OwnerController->GetPlayerViewPoint(Start, Rotation);

	float Spread = CalculateBulletSpread();
	float SpreadInRadians = FMath::DegreesToRadians(Spread);

	FVector SpreadDirection = FMath::VRandCone(Rotation.Vector(), SpreadInRadians);
	FVector FinalEnd = Start + (SpreadDirection * CurrentWeaponData.MaxRange);

	PlayWeaponEffects();
	ServerHandleFire(Start, FinalEnd);

	if (CurrentAmmo <= 0 || CurrentWeaponData.FireMode == EWeaponFireMode::Single)
	{
		StopFire();
		if (CurrentAmmo <= 0) Reload();
	}
}

void ABaseWeapon::ServerHandleFire_Implementation(FVector StartLocation, FVector EndLocation)
{
	if (CurrentAmmo <= 0 || bIsReloading) return;

	float DistanceSquared = FVector::DistSquared(StartLocation, EndLocation);
	float MaxRangeWithBuffer = CurrentWeaponData.MaxRange + 100.f;
	if (DistanceSquared >= FMath::Square(MaxRangeWithBuffer)) return;

	AController* DamageInstigator = OwnerCharacter ? OwnerCharacter->GetController() : nullptr;

	if (GetNetMode() == NM_DedicatedServer || (OwnerCharacter && !OwnerCharacter->IsLocallyControlled()))
	{
		CurrentAmmo--;
	}

	BurstCounter++;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(GetOwner());

	if (CurrentWeaponData.MaxZombiePenetration <= 1)
	{
		FHitResult Hit;
		bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, StartLocation, EndLocation, ECC_Bullet, Params);
		if (bHit)
		{
			UGameplayStatics::ApplyPointDamage(Hit.GetActor(), CurrentWeaponData.Damage, Hit.ImpactNormal, Hit, DamageInstigator, this, UDamageType::StaticClass());
		}
	}
	else
	{
		TArray<FHitResult> Hits;
		GetWorld()->LineTraceMultiByChannel(Hits, StartLocation, EndLocation, ECC_Bullet, Params);

		int32 PenetrationCount = 0;
		TSet<AActor*> HitActors;

		for (const FHitResult& Hit : Hits)
		{
			if (Hit.GetActor() && !HitActors.Contains(Hit.GetActor()))
			{
				UGameplayStatics::ApplyPointDamage(Hit.GetActor(), CurrentWeaponData.Damage, Hit.ImpactNormal, Hit, DamageInstigator, this, UDamageType::StaticClass());
				HitActors.Add(Hit.GetActor());
				PenetrationCount++;

				if (PenetrationCount >= CurrentWeaponData.MaxZombiePenetration) break;
			}
		}
	}

	FVector MuzzleLocation = WeaponMesh->GetSocketLocation(CurrentWeaponData.MuzzleLocationName);
	DrawDebugLine(GetWorld(), MuzzleLocation, EndLocation, FColor::Red, false, 2.f, 0.f, 1.f);
}

void ABaseWeapon::OnRep_BurstCounter()
{
	PlayWeaponEffects();
}

void ABaseWeapon::PlayWeaponEffects()
{
	// لێرەدا ئەفێکتەکان لێدەدرێن
}

void ABaseWeapon::Reload()
{
	if (ReserveAmmo <= 0 || bIsReloading || CurrentAmmo == CurrentWeaponData.MaxAmmoInClip || !OwnerCharacter || !OwnerCharacter->IsLocallyControlled()) return;

	bIsReloading = true;
	float ReloadTimeToUse = CurrentWeaponData.DefaultReloadTime;

	if (CurrentWeaponData.ReloadMontage)
	{
		OwnerCharacter->PlayAnimMontage(CurrentWeaponData.ReloadMontage);
		ReloadTimeToUse = CurrentWeaponData.ReloadMontage->GetPlayLength();
	}

	GetWorldTimerManager().SetTimer(ReloadTimerHandle, this, &ABaseWeapon::Local_ReloadComplete, ReloadTimeToUse, false);

	if (HasAuthority())
	{
		ServerReload_Implementation(ReloadTimeToUse);
	}
	else
	{
		ServerReload(ReloadTimeToUse);
	}
}

void ABaseWeapon::Local_ReloadComplete()
{
	if (HasAuthority()) return;

	int32 AmmoNeeded = CurrentWeaponData.MaxAmmoInClip - CurrentAmmo;
	int32 AmmoToMove = FMath::Min(AmmoNeeded, ReserveAmmo);

	CurrentAmmo += AmmoToMove;
	ReserveAmmo -= AmmoToMove;
	bIsReloading = false;

	if (OwnerCharacter && OwnerCharacter->bIsFireButtonHold)
	{
		StartFire();
	}
}

void ABaseWeapon::ServerReload_Implementation(float InReloadTime)
{
	bIsReloading = true;

	if (OwnerCharacter && !OwnerCharacter->IsLocallyControlled())
	{
		OnRep_Reload();
	}

	float ExactReloadTime = CurrentWeaponData.ReloadMontage ? CurrentWeaponData.ReloadMontage->GetPlayLength() : InReloadTime;
	float BufferTolerance = (OwnerCharacter && OwnerCharacter->IsLocallyControlled()) ? ExactReloadTime : FMath::Max(ExactReloadTime - 0.2f, 0.1f);

	GetWorldTimerManager().SetTimer(ReloadTimerHandle, this, &ABaseWeapon::Server_ReloadComplete, BufferTolerance, false);
}

void ABaseWeapon::Server_ReloadComplete()
{
	int32 AmmoNeeded = CurrentWeaponData.MaxAmmoInClip - CurrentAmmo;
	int32 AmmoToMove = FMath::Min(AmmoNeeded, ReserveAmmo);

	CurrentAmmo += AmmoToMove;
	ReserveAmmo -= AmmoToMove;
	bIsReloading = false;

	if (OwnerCharacter && !OwnerCharacter->IsLocallyControlled())
	{
		OnRep_Reload();
	}

	if (OwnerCharacter && OwnerCharacter->IsLocallyControlled() && OwnerCharacter->bIsFireButtonHold)
	{
		StartFire();
	}
}

void ABaseWeapon::CancelReload()
{
	GetWorldTimerManager().ClearTimer(ReloadTimerHandle);

	if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
	{
		bIsReloading = false;
		USkeletalMeshComponent* MeshComp = OwnerCharacter->GetMesh();
		if (MeshComp && MeshComp->GetAnimInstance())
		{
			MeshComp->GetAnimInstance()->Montage_Stop(0.2f, CurrentWeaponData.ReloadMontage);
		}

		if (!HasAuthority())
		{
			Server_CancelReload();
		}
	}

	if (HasAuthority())
	{
		bIsReloading = false;
		if (OwnerCharacter && !OwnerCharacter->IsLocallyControlled())
		{
			USkeletalMeshComponent* MeshComp = OwnerCharacter->GetMesh();
			if (MeshComp && MeshComp->GetAnimInstance())
			{
				MeshComp->GetAnimInstance()->Montage_Stop(0.2f, CurrentWeaponData.ReloadMontage);
			}
		}
	}
}

void ABaseWeapon::Server_CancelReload_Implementation()
{
	bIsReloading = false;
	GetWorldTimerManager().ClearTimer(ReloadTimerHandle);

	if (OwnerCharacter && !OwnerCharacter->IsLocallyControlled())
	{
		OnRep_Reload();
		USkeletalMeshComponent* MeshComp = OwnerCharacter->GetMesh();
		if (MeshComp && MeshComp->GetAnimInstance())
		{
			MeshComp->GetAnimInstance()->Montage_Stop(0.2f, CurrentWeaponData.ReloadMontage);
		}
	}
}

void ABaseWeapon::OnRep_Reload()
{
	if (!OwnerCharacter || OwnerCharacter->IsLocallyControlled()) return;

	USkeletalMeshComponent* MeshComp = OwnerCharacter->GetMesh();
	if (!MeshComp) return;

	UAnimInstance* AnimInstance = MeshComp->GetAnimInstance();
	if (!AnimInstance) return;

	if (bIsReloading)
	{
		if (CurrentWeaponData.ReloadMontage) AnimInstance->Montage_Play(CurrentWeaponData.ReloadMontage);
	}
	else
	{
		if (CurrentWeaponData.ReloadMontage) AnimInstance->Montage_Stop(0.2f, CurrentWeaponData.ReloadMontage);
	}
}