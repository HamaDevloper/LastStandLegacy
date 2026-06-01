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
	if (CurrentAmmo <= 0 && ReserveAmmo <= 0) return;
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

	if (HamaComponent && HamaComponent->IsFiring())
	{
		HamaComponent->SetFiring(false);
	}
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

	// ١. پشکنینی فیشەک لە سەرەتای فەنکشنەکە (گرنگە!)
	if (CurrentAmmo <= 0)
	{
		StopFire();
		Reload();
		return;
	}

	if (!OwnerController)
	{
		OwnerController = Cast<APlayerController>(OwnerCharacter->GetController());
		if (!OwnerController) return;
	}

	CurrentAmmo--;
	LastFireTime = GetWorld()->GetTimeSeconds();

	// ڕاگرتنی ڕاکردن و دەستپێکردنی ئەنیمەیشنی تەقە
	if (HamaComponent->bIsSprinting) HamaComponent->StopSprinting();
	if (!HamaComponent->IsFiring()) HamaComponent->SetFiring(true);

	// حیسابکردنی ئاڕاستەی فیشەک و بڵاوبوونەوە (Spread)
	FVector Start;
	FRotator Rotation;
	OwnerController->GetPlayerViewPoint(Start, Rotation);

	float Spread = CalculateBulletSpread();
	float SpreadInRadians = FMath::DegreesToRadians(Spread);

	FVector SpreadDirection = FMath::VRandCone(Rotation.Vector(), SpreadInRadians);
	FVector FinalEnd = Start + (SpreadDirection * MaxRange);

	// کایکردنی دەنگ و افێکت کڵایەنت
	PlayWeaponEffects();

	// ناردنی زانیاری بۆ سێرڤەر بۆ دروستکردنی هێڵی پێکان (Trace)
	ServerHandleFire(Start, FinalEnd);

	// ٣. ئەگەر ئەمە کۆتا فیشەک بوو کە تەقێندرا، یەکسەر لێرەدا ڕیلۆد بکە بۆ فیشەکی داهاتوو
	if (CurrentAmmo <= 0)
	{
		StopFire();
		Reload();
	}
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
		bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, StartLocation, EndLocation, ECC_Bullet, Params);
		if (bHit)
		{
			UGameplayStatics::ApplyPointDamage(Hit.GetActor(), Damage, Hit.ImpactNormal, Hit, DamageInstigator, this, UDamageType::StaticClass());
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
	// چاککردنی مەرجەکە: ئەگەر هۆست بوو، با لێرە نەیگەڕێنێتەوە، تەنها دڵنیابە لەوەی bIsReloading چالاکە
	bIsReloading = true;

	// ناردنی سیگناڵ بۆ کڵایەنتەکانی تر (Simulated Proxies) بۆ ئەوەی ئەنیمەیشنەکە بببینن
	if (OwnerCharacter && !OwnerCharacter->IsLocallyControlled())
	{
		// ئەگەر bIsReloading ڕیپلیکەیت کرابێت، OnRep_Reload خۆکارانە لای کڵایەنتەکان بانگ دەبێت،
		// بەڵام بۆ دڵنیایی زیاتر لێرە بە دەستی بانگی دەکەین بۆ کڵایەنتەکانی تر
		OnRep_Reload();
	}

	// پارێزگاری دژە هاک (Anti-Cheat)
	float ExactReloadTime = ReloadMontage ? ReloadMontage->GetPlayLength() : InReloadTime;

	// ئەگەر خۆمان هۆست بووین (Locally Controlled)، با کاتەکە ڕێک وەک خۆی بێت و کەم نەبێتەوە
	float BufferTolerance = (OwnerCharacter && OwnerCharacter->IsLocallyControlled()) ? ExactReloadTime : FMath::Max(ExactReloadTime - 0.2f, 0.1f);

	// دانانی کات بۆ کۆتایی ڕیلۆدی سێرڤەر (ئەمە لای هۆست و لای سێرڤەریش ڕەن دەبێت)
	GetWorldTimerManager().SetTimer(ReloadTimerHandle, this, &ABaseWeapon::Server_ReloadComplete, BufferTolerance, false);
}

void ABaseWeapon::Server_ReloadComplete()
{
	// لای هۆست یان کڵایەنت، لۆژیکی پڕکردنەوەی فیشەکەکە لێرە جێبەجێ دەبێت
	int32 AmmoNeeded = MaxAmmoInClip - CurrentAmmo;
	int32 AmmoToMove = FMath::Min(AmmoNeeded, ReserveAmmo);

	CurrentAmmo += AmmoToMove;
	ReserveAmmo -= AmmoToMove;
	bIsReloading = false;

	// کاتێک ڕیلۆد تەواو دەبێت، دڵنیابوونەوە لە ناردنی نوێکاری بۆ هەمووان (ئەگەر پێویست بکات)
	if (OwnerCharacter && !OwnerCharacter->IsLocallyControlled())
	{
		OnRep_Reload(); // بۆ ڕاگرتنی ئەنیمەیشن لای کەسانی تر
	}

	// ئەگەر یاریزانەکە (چ هۆست چ کڵایەنت) هێشتا دەستی لەسەر کلیکی تەقە بوو، با دیسان دەست بە تەقە بکاتەوە
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