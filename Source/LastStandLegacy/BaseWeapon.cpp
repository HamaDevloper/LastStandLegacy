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

	// فێڵی زیرەک: بژمێرەکان دەدرێتە Simulated Proxies، یاریزانی خاوەن پشت بە خێرایی کۆمپیوتەری خۆی دەبەستێت تا لاگ دروست نەبێت
	DOREPLIFETIME_CONDITION(ABaseWeapon, bIsReloading, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(ABaseWeapon, CurrentAmmo, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ABaseWeapon, MaxAmmoInClip, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ABaseWeapon, ReserveAmmo, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ABaseWeapon, BurstCounter, COND_SkipOwner);
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

	float CurrentTime = GetWorld()->GetTimeSeconds();

	if (CurrentTime - LastFireTime >= FireRate)
	{
		HandleFireLocal();
	}

	if (FireMode == EWeaponFireMode::Automatic || FireMode == EWeaponFireMode::Burst)
	{
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

	LastFireTime = GetWorld()->GetTimeSeconds();

	// پێشبینی تەقەی لۆکاڵ بەمە ناتگێڕێتەوە دواوە گەر پینگ زۆریش بێت
	CurrentAmmo--;

	if (HamaComponent->bIsSprinting) HamaComponent->StopSprinting();
	if (!HamaComponent->IsFiring()) HamaComponent->SetFiring(true);

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
	if (CurrentAmmo <= 0 || bIsReloading) return;

	float DistanceSquared = FVector::DistSquared(StartLocation, EndLocation);
	float MaxRangeWithBuffer = MaxRange + 100.f;
	if (DistanceSquared >= FMath::Square(MaxRangeWithBuffer))
	{
		return;
	}

	AController* DamageInstigator = OwnerCharacter ? OwnerCharacter->GetController() : nullptr;

	if (GetNetMode() == NM_DedicatedServer || (OwnerCharacter && !OwnerCharacter->IsLocallyControlled()))
	{
		CurrentAmmo--;
	}

	BurstCounter++;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(GetOwner());

	if (MaxZombiePenetration <= 1)
	{
		FHitResult Hit;
		bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, StartLocation, EndLocation, ECC_Zombie, Params);
		if (bHit)
		{
			UGameplayStatics::ApplyPointDamage(Hit.GetActor(), Damage, Hit.ImpactNormal, Hit, DamageInstigator, this, UDamageType::StaticClass());
		}
	}
	else
	{
		TArray<FHitResult> Hits;
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
}

void ABaseWeapon::OnRep_BurstCounter()
{
	PlayWeaponEffects();
}

void ABaseWeapon::PlayWeaponEffects()
{
	// تێبینی: دەنگەکان و پارتیکڵەکانت (Particles) لێرە دەکەیتە ڕەن. Muzzle flash یان دەنگ...
}

void ABaseWeapon::Reload()
{
	if (ReserveAmmo <= 0 || bIsReloading || CurrentAmmo == MaxAmmoInClip || !OwnerCharacter || !OwnerCharacter->IsLocallyControlled()) return;

	// خۆی دایدەنێتەوە بێ ئەوەی چاوەڕێی سەرڤەر بێت! ئەمە چارەسەری بەستن دەکات بۆ خاوەنەکە
	bIsReloading = true;
	float ReloadTimeToUse = DefaultReloadTime;

	if (ReloadMontage)
	{
		OwnerCharacter->PlayAnimMontage(ReloadMontage);
		ReloadTimeToUse = ReloadMontage->GetPlayLength(); // هێنانەدەری درێژی ئەنیمەیشن
	}

	// لای خۆمان تایمەری دەدەینێ تا ژمارەکان یەکسەر وەهمی بپەڕنە سەر تا دروستکەر ڕەتیدەکاتەوە
	GetWorldTimerManager().SetTimer(ReloadTimerHandle, this, &ABaseWeapon::Local_ReloadComplete, ReloadTimeToUse, false);

	if (HasAuthority())
	{
		ServerReload_Implementation(ReloadTimeToUse);
	}
	else
	{
		// درێژی کاتەکە بۆ سێرڤەر ئەنێرین، یاخود دەکرێ سێرڤەر خۆیشی دەریبکات لە کۆدەکەی دواتر
		ServerReload(ReloadTimeToUse);
	}
}

// کاتێک یاریزانی خاوەن کۆتایی بە ئەنیمەیشن دێت بەپێی کات، پێشبینی لۆکاڵی تەواو دەکات
void ABaseWeapon::Local_ReloadComplete()
{
	if (HasAuthority()) return; // چونکە گەر هۆست بووین سێرڤەر دەیکات بۆ خۆی لە ڕێی Server_ReloadComplete

	int32 AmmoNeeded = MaxAmmoInClip - CurrentAmmo;
	int32 AmmoToMove = FMath::Min(AmmoNeeded, ReserveAmmo);

	CurrentAmmo += AmmoToMove;
	ReserveAmmo -= AmmoToMove;
	bIsReloading = false;

	if (OwnerCharacter && OwnerCharacter->bIsFireButtonHold)
	{
		StartFire(); // ئەگەر ماوسی گرتبوو کە کۆتایی هات با دیسان ئۆتۆ تەقە بکات
	}
}

void ABaseWeapon::ServerReload_Implementation(float InReloadTime)
{
	// دڵنیایی کە خێرایی نەگەڕێنێتەوە، و ئەگەر خۆی هۆست نەبوو دووبارەی نەکاتەوە 
	if (bIsReloading && HasAuthority() && OwnerCharacter && OwnerCharacter->IsLocallyControlled())
		return;

	bIsReloading = true;

	// ناردنی سیگناڵ بۆ کەسانی ناو سێرڤەر کە من ئەنیمەیشن لێئەدەم!
	if (OwnerCharacter && !OwnerCharacter->IsLocallyControlled())
	{
		OnRep_Reload();
	}

	// پارێزگارییەکە با هاککەر خێرایی کەم نەگەیەنێت
	float ExactReloadTime = ReloadMontage ? ReloadMontage->GetPlayLength() : InReloadTime;
	float BufferTolerance = FMath::Max(ExactReloadTime - 0.2f, 0.1f); // ڕێگە دان بە کەمێ جیاوازی لاگ (Anti-Cheat Validation)

	// دانانی کات بۆ کۆتایی ڕیلۆد
	GetWorldTimerManager().SetTimer(ReloadTimerHandle, this, &ABaseWeapon::Server_ReloadComplete, BufferTolerance, false);
}

void ABaseWeapon::Server_ReloadComplete()
{
	int32 AmmoNeeded = MaxAmmoInClip - CurrentAmmo;
	int32 AmmoToMove = FMath::Min(AmmoNeeded, ReserveAmmo);

	CurrentAmmo += AmmoToMove;
	ReserveAmmo -= AmmoToMove;
	bIsReloading = false;

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
			MeshComp->GetAnimInstance()->Montage_Stop(0.2f, ReloadMontage);
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
				MeshComp->GetAnimInstance()->Montage_Stop(0.2f, ReloadMontage);
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
		USkeletalMeshComponent* MeshComp = OwnerCharacter->GetMesh();
		if (MeshComp && MeshComp->GetAnimInstance())
		{
			MeshComp->GetAnimInstance()->Montage_Stop(0.2f, ReloadMontage);
		}
	}
}

// تەنها بۆ کەسانی (دیتر) وەك Proxies ڕەن دەبێت کاتێ سەرڤەر گووتی "خەریکە پرئەکاتەوە"
void ABaseWeapon::OnRep_Reload()
{
	if (!OwnerCharacter || OwnerCharacter->IsLocallyControlled()) return;

	USkeletalMeshComponent* MeshComp = OwnerCharacter->GetMesh();
	if (!MeshComp) return;

	UAnimInstance* AnimInstance = MeshComp->GetAnimInstance();
	if (!AnimInstance) return;

	if (bIsReloading)
	{
		if (ReloadMontage) AnimInstance->Montage_Play(ReloadMontage);
	}
	else
	{
		if (ReloadMontage) AnimInstance->Montage_Stop(0.2f, ReloadMontage);
	}
}