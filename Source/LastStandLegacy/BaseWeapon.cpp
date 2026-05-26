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

	// bIsReloading لێرەدا کرا بە بێ مەرج بۆ ئەوەی کڵاێنتەکانی تریش ئاگاداری ڕیلۆد ببن و ئەنیمەیشنەکە لێبدات
	DOREPLIFETIME(ABaseWeapon, bIsReloading);

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
	if (HamaComponent && HamaComponent->bIsAiming)
	{
		return 0.f;
	}

	float CurrentSpread = BulletSpread;

	if (OwnerCharacter && OwnerCharacter->GetCharacterMovement())
	{
		UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement();

		if (MoveComp->IsCrouching())
		{
			CurrentSpread *= CrouchSpreadMultiplier;
		}
		else if (MoveComp->IsFalling())
		{
			CurrentSpread *= AirSpreadMultiplier;
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

		if (HamaComponent)
		{
			HamaComponent->SetFiring(false);
		}

		Reload();
		return;
	}

	if (!OwnerController)
	{
		OwnerController = Cast<APlayerController>(OwnerCharacter->GetController());
		if (!OwnerController) return;
	}

	if (HamaComponent->bIsSprinting) HamaComponent->StopSprinting();
	HamaComponent->SetFiring(true);

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
	// ⚠️ پاراستنی سێرڤەر: ئەگەر فیشەک نەمابوو یان لە حاڵەتی ڕیلۆدابوو، سێرڤەر ڕێگا نادات تەقە بکرێت
	if (CurrentAmmo <= 0 || bIsReloading) return;

	AController* DamageInstigator = OwnerCharacter ? OwnerCharacter->GetController() : nullptr;
	FVector LineVisualEnd = EndLocation;

	CurrentAmmo--;

	if (MaxZombiePenetration <= 1)
	{
		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);
		Params.AddIgnoredActor(GetOwner());

		bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, StartLocation, EndLocation, ECC_Zombie, Params);
		if (bHit)
		{
			UGameplayStatics::ApplyPointDamage(Hit.GetActor(), Damage, Hit.ImpactNormal, Hit, DamageInstigator, this, UDamageType::StaticClass());
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
				UGameplayStatics::ApplyPointDamage(Hit.GetActor(), Damage, Hit.ImpactNormal, Hit, DamageInstigator, this, UDamageType::StaticClass());
				HitActors.Add(Hit.GetActor());
				PenetrationCount++;
				LineVisualEnd = Hit.ImpactPoint;

				if (PenetrationCount >= MaxZombiePenetration) break;
			}
		}
	}

	FVector MuzzleLocation = WeaponMesh->GetSocketLocation(MuzzleLocationName);
	DrawDebugLine(GetWorld(), MuzzleLocation, LineVisualEnd, FColor::Red, false, 2.f, 0.f, 3.f);

	MulticastPlayFireEffects();

	// ئەگەر دوای ئەم فیشەکە چەکەکە خاڵی بووەوە، سێرڤەر خۆی ڕیلۆدەکە دەستپێدەکات
	if (CurrentAmmo <= 0)
	{
		Reload();
	}
}

void ABaseWeapon::MulticastPlayFireEffects_Implementation()
{
	if (OwnerCharacter && OwnerCharacter->IsLocallyControlled()) return;
	PlayWeaponEffects();
}

void ABaseWeapon::PlayWeaponEffects()
{
}

void ABaseWeapon::Reload()
{
	if (bIsReloading || CurrentAmmo == MaxAmmoInClip) return;

	bIsReloading = true;
	float ReloadTime = 1.5f;

	if (ReloadMontage && OwnerCharacter)
	{
		ReloadTime = OwnerCharacter->PlayAnimMontage(ReloadMontage);
	}

	if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
	{
		// ئەگەر خۆمان کڵاێنت بووین RPC دەنێرین، ئەگەر خۆمان سێرڤەر بووین ڕاستەوخۆ لۆجیکەکە جێبەجێ دەکەین
		if (!HasAuthority())
		{
			ServerReload(ReloadTime);
		}
		else
		{
			ServerReload_Implementation(ReloadTime);
		}
	}
}

void ABaseWeapon::ServerReload_Implementation(float ReloadTime)
{
	bIsReloading = true;

	// کڵاێنتەکانی تر ئاگادار دەکەینەوە کە ئەنیمەیشن لێبدەن
	if (OwnerCharacter && !OwnerCharacter->IsLocallyControlled())
	{
		OnRep_Reload();
	}

	GetWorldTimerManager().SetTimer(ReloadTimer, this, &ABaseWeapon::ReloadFinish, ReloadTime, false);
}

void ABaseWeapon::ReloadFinish()
{
	bIsReloading = false;

	int32 AmmoNeeded = MaxAmmoInClip - CurrentAmmo;
	int32 AmmoToMove = FMath::Min(AmmoNeeded, ReserveAmmo);

	CurrentAmmo += AmmoToMove;
	ReserveAmmo -= AmmoToMove;

	if (OwnerCharacter)
	{
		OnRep_Reload();
	}

	if (OwnerCharacter->IsLocallyControlled() && OwnerCharacter->bIsFireButtonHold)
	{
		StartFire();
	}
}

void ABaseWeapon::OnRep_Reload()
{
	if (!OwnerCharacter) return;
	if (OwnerCharacter->IsLocallyControlled())
	{
		if (!bIsReloading && OwnerCharacter->bIsFireButtonHold)
		{
			StartFire();
		}
		return;
	}

	USkeletalMeshComponent* MeshComp = OwnerCharacter->GetMesh();
	if (!MeshComp) return;

	UAnimInstance* AnimInstance = MeshComp->GetAnimInstance();
	if (!AnimInstance) return;

	if (bIsReloading)
	{
		if (ReloadMontage)
		{
			AnimInstance->Montage_Play(ReloadMontage);
		}
	}
	else
	{
		if (ReloadMontage)
		{
			AnimInstance->Montage_Stop(0.2f, ReloadMontage);
		}
	}
}