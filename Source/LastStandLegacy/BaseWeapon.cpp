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

	DOREPLIFETIME(ABaseWeapon, bIsReloading);
	DOREPLIFETIME_CONDITION(ABaseWeapon, CurrentAmmo, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ABaseWeapon, MaxAmmoInClip, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ABaseWeapon, ReserveAmmo, COND_OwnerOnly);

	// فێڵی زیرەک: بژمێرەکە بە کڵایەنتەکە خۆی نادەین چونکە پێشتر دەنگەکەی لە HandleFireLocal بۆ خۆی لێداوە
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

	// بە ڕێگەیەکی زۆر خێرا سەیری دەکات، ئەگەر دوو کرتەی خێرای کرد و کاتی فیشەکی نەهاتبوو تەقە نەکات 
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

	// ڕاستەوخۆ کەمکردنەوە (Prediction بۆ کڵایەنت تا لاگ نەبێت)
	CurrentAmmo--;

	if (HamaComponent->bIsSprinting) HamaComponent->StopSprinting();
	if(!HamaComponent->IsFiring()) HamaComponent->SetFiring(true);

	FVector Start;
	FRotator Rotation;
	OwnerController->GetPlayerViewPoint(Start, Rotation);

	float Spread = CalculateBulletSpread();
	float SpreadInRadians = FMath::DegreesToRadians(Spread);

	FVector SpreadDirection = FMath::VRandCone(Rotation.Vector(), SpreadInRadians);
	FVector FinalEnd = Start + (SpreadDirection * MaxRange);

	// تەقە ڕاستەوخۆ دیار دەبێت بۆ یاریزانەکە
	PlayWeaponEffects();

	// ئەگەر خۆت سێرڤەر نەبیت دەتوانیت Line Traceـیەکی بچووکیش بۆ پارتیکڵی سەر زۆمبی بکەیت تا نەوەستیت

	ServerHandleFire(Start, FinalEnd);
}

void ABaseWeapon::ServerHandleFire_Implementation(FVector StartLocation, FVector EndLocation)
{
	// پاراستن
	if (CurrentAmmo <= 0 || bIsReloading) return;

	AController* DamageInstigator = OwnerCharacter ? OwnerCharacter->GetController() : nullptr;

	if (GetNetMode() == NM_DedicatedServer || (OwnerCharacter && !OwnerCharacter->IsLocallyControlled()))
	{
		CurrentAmmo--;
	}

	// بژمێری ئەفێکت زیاد دەکەین تا ئەوانی تریش بیگۆڕن
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

// کاتێک بژمێرەکەی سێرڤەر زیاد دەبێت، بۆ simulated proxies ەکان ڕەن دەبێت. بۆ خاوەنی چەک نا، چونکە لۆکاڵ دەنگەکەی ژەند
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
	if (ReserveAmmo <= 0 || bIsReloading || CurrentAmmo == MaxAmmoInClip) return;

	bIsReloading = true;

	if (ReloadMontage && OwnerCharacter && OwnerCharacter->IsLocallyControlled())
	{
		OwnerCharacter->PlayAnimMontage(ReloadMontage);
	}

	if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
	{
		if (!HasAuthority())
		{
			ServerReload();
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

	// تایمەر لابرا! لەجیاتی ئەوە کڵاێنت کاتێک مۆنتاژەکەی گەیشتە کاتی (Anim Notify)، بانگی ReloadFinish دەکات.
	// سێرڤەریش ئەگەر Simulated ەکانی کرد بێت خۆیان ڕوودەدەن.

	if (OwnerCharacter && !OwnerCharacter->IsLocallyControlled())
	{
		OnRep_Reload();
	}
}

void ABaseWeapon::ReloadFinish()
{
	// ئەم فەنکشنە پێویستە لەلایەن (Anim Notify) لە بلوپرینتی مۆنتاژەکە (کاتێک دەستی ئەچێتە ناو مەخزەنەکە) بانگ بکرێت 

	if (HasAuthority())
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
	else
	{
		// پێش سەلماندنی سێرڤەر ڕاستەوخۆ خۆی بۆی پڕ دەکات (وەهمی)، پاشان بۆ سێرڤەری دەنێرێت
		int32 AmmoNeeded = MaxAmmoInClip - CurrentAmmo;
		int32 AmmoToMove = FMath::Min(AmmoNeeded, ReserveAmmo);
		CurrentAmmo += AmmoToMove;
		ReserveAmmo -= AmmoToMove;
		bIsReloading = false;

		Server_FinishReloadValidation();

		if (OwnerCharacter && OwnerCharacter->IsLocallyControlled() && OwnerCharacter->bIsFireButtonHold)
		{
			StartFire();
		}
	}
}

void ABaseWeapon::Server_FinishReloadValidation_Implementation()
{
	if (!bIsReloading) return;

	int32 AmmoNeeded = MaxAmmoInClip - CurrentAmmo;
	int32 AmmoToMove = FMath::Min(AmmoNeeded, ReserveAmmo);

	CurrentAmmo += AmmoToMove;
	ReserveAmmo -= AmmoToMove;
	bIsReloading = false;
}

void ABaseWeapon::CancelReload()
{
	if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
	{
		USkeletalMeshComponent* MeshComp = OwnerCharacter->GetMesh();
		if (MeshComp && MeshComp->GetAnimInstance())
		{
			MeshComp->GetAnimInstance()->Montage_Stop(0.2f, ReloadMontage);
		}
	}

	if (HasAuthority())
	{
		bIsReloading = false;
	}
	else
	{
		Server_CancelReload();
	}
}

void ABaseWeapon::Server_CancelReload_Implementation()
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

void ABaseWeapon::OnRep_Reload()
{
	if (!OwnerCharacter) return;

	if (OwnerCharacter->IsLocallyControlled()) return;

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