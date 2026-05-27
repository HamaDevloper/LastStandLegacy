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
	if (bIsReloading)
	{
		if (CurrentAmmo > 0) CancelReload();
		else return;
	}

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

	// ئەگەر فیشەک نەمابوو وەستاندن و ڕیلۆد
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

	
	CurrentAmmo--;

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
	// ⚠️ پاراستنی سێرڤەر
	if (CurrentAmmo <= 0 || bIsReloading) return;

	AController* DamageInstigator = OwnerCharacter ? OwnerCharacter->GetController() : nullptr;

	if (GetNetMode() == NM_DedicatedServer || (OwnerCharacter && !OwnerCharacter->IsLocallyControlled()))
	{
		CurrentAmmo--;
	}

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

				if (PenetrationCount >= MaxZombiePenetration) break;
			}
		}
	}

	DrawDebugLine(GetWorld(), StartLocation, EndLocation, FColor::Red, false, 2.f, 0.f, 1.f);

	MulticastPlayFireEffects();
}

void ABaseWeapon::MulticastPlayFireEffects_Implementation()
{
	if (OwnerCharacter && OwnerCharacter->IsLocallyControlled()) return;
	PlayWeaponEffects();
}

void ABaseWeapon::PlayWeaponEffects()
{
	// تێبینی: دەنگەکان و پارتیکڵەکانت (Particles) لێرە دەکەیتە ڕەن.
}

void ABaseWeapon::Reload()
{
	if (ReserveAmmo <= 0 || bIsReloading || CurrentAmmo == MaxAmmoInClip) return;

	bIsReloading = true;

	// کڵاێنت مۆنتاژەکە لە لای خۆی (لۆکاڵی) دەردەهێنێتەوە تا ئەنیمەیشن پێشبینی (Predict) بکات بە خێرایی
	if (ReloadMontage && OwnerCharacter && OwnerCharacter->IsLocallyControlled())
	{
		OwnerCharacter->PlayAnimMontage(ReloadMontage);
	}

	if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
	{
		// پەیوەندی بە سێرڤەر دەکەین بەڵام چیدی خێرایی مۆنتاژی پێ نادەین! 
		if (!HasAuthority())
		{
			ServerReload(); // لایبە پارامیتەرەکە تا ڕێگا بە دروستکردنی فێڵ نەکەیت!
		}
		else
		{
			ServerReload_Implementation();
		}
	}
}

void ABaseWeapon::ServerReload_Implementation()
{
	bIsReloading = true;

	// 📌 چارەسەری دووەم: سێرڤەر خۆی تایمی ئەنیمەیشن دەستنیشان دەکات نەک یاریزان کاتی هەڵە (Hack) بینێرێت.
	float SecureReloadTime = 1.5f; // ئەگەر ئەنیمەیشنت نەبوو
	if (ReloadMontage)
	{
		SecureReloadTime = ReloadMontage->GetPlayLength(); // درێژی مۆنتاژە بە سێکورەی سێرڤەر دۆزرایەوە.
	}

	// کڵاێنتەکانی تر ئاگادار دەکەینەوە کە ئەنیمەیشن لێبدەن
	if (OwnerCharacter && !OwnerCharacter->IsLocallyControlled())
	{
		OnRep_Reload();
	}

	GetWorldTimerManager().SetTimer(ReloadTimer, this, &ABaseWeapon::ReloadFinish, SecureReloadTime, false);
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

	if (OwnerCharacter && OwnerCharacter->IsLocallyControlled() && OwnerCharacter->bIsFireButtonHold)
	{
		StartFire();
	}
}

void ABaseWeapon::CancelReload()
{
	if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
	{
		USkeletalMeshComponent* MeshComp = OwnerCharacter->GetMesh();
		if (MeshComp)
		{
			MeshComp->GetAnimInstance()->Montage_Stop(0.2f, ReloadMontage);
		}
	}

	if (HasAuthority())
	{
		bIsReloading = false;
		GetWorldTimerManager().ClearTimer(ReloadTimer);
	}
	else
	{
		Server_CancelReload();
	}
}

void ABaseWeapon::Server_CancelReload_Implementation()
{
	bIsReloading = false;
	GetWorldTimerManager().ClearTimer(ReloadTimer);

	if (OwnerCharacter && !OwnerCharacter->IsLocallyControlled())
	{
		USkeletalMeshComponent* MeshComp = OwnerCharacter->GetMesh();
		if (MeshComp)
		{
			MeshComp->GetAnimInstance()->Montage_Stop(0.2f, ReloadMontage);
		}
	}
}

void ABaseWeapon::OnRep_Reload()
{
	if (!OwnerCharacter) return;

	// کڵایەنتی سەرەکی پێویستی بەم بەشەی خوارەوە نییە چونکە لە CancelReload لای خۆی ڕایگرتووە
	if (OwnerCharacter->IsLocallyControlled())
	{
		if (!bIsReloading && OwnerCharacter->bIsFireButtonHold)
		{
			StartFire(); // ئەگەر دەستی لەسەر تەقەکردن بوو، ڕاستەوخۆ دەست بە تەقە بکاتەوە
		}
		return;
	}

	// ئەم بەشە تەنها بۆ یاریزانەکانی ترە (Simulated Proxies) بۆ ئەوەی ئەنیمەیشنی یەکتر ببینن
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