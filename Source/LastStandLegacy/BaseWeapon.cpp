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
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	RootComponent = WeaponMesh;

	LastFireTime = 0.f;
}

void ABaseWeapon::BeginPlay()
{
	Super::BeginPlay();

	InitializeWeaponData();

	OwnerCharacter = Cast<AHama>(GetOwner());
	if (OwnerCharacter)
	{
		HamaComponent = OwnerCharacter->FindComponentByClass<UHamaComponent>();
		OwnerController = Cast<APlayerController>(OwnerCharacter->GetController());
	}
}

void ABaseWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!OwnerController || !OwnerCharacter || !OwnerCharacter->IsLocallyControlled()) return;

	FRotator Previous = CurrentRecoilOffset;
	CurrentRecoilOffset = FMath::RInterpTo(CurrentRecoilOffset, TargetRecoilOffset, DeltaTime, 12.0f);

	float DeltaPitch = CurrentRecoilOffset.Pitch - Previous.Pitch;
	float DeltaYaw = CurrentRecoilOffset.Yaw - Previous.Yaw;

	OwnerController->AddPitchInput(-DeltaPitch);
	OwnerController->AddYawInput(DeltaYaw);

	if (CurrentRecoilOffset.Equals(TargetRecoilOffset, 0.01f))
	{
		SetActorTickEnabled(false);
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

			MaxAmmoInClip = CurrentWeaponData.MaxAmmoInClip;
			CurrentAmmo = MaxAmmoInClip;
			ReserveAmmo = CurrentWeaponData.MaxReserveAmmo;

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
	}

	ResetRecoil();
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

	if (HamaComponent)
	{
		if (HamaComponent->bIsSprinting) HamaComponent->StopSprinting();
	}

	if (ShootForceFeedback)
	{
		FForceFeedbackParameters FeedbackParams;
		FeedbackParams.bLooping = false;
		FeedbackParams.bIgnoreTimeDilation = false;
		FeedbackParams.Tag = FName("WeaponFire");
		OwnerController->ClientPlayForceFeedback(ShootForceFeedback, FeedbackParams);
	}

	FVector CameraLoc;
	FRotator CameraRot;
	OwnerController->GetPlayerViewPoint(CameraLoc, CameraRot);

	float Spread = CalculateBulletSpread();
	float SpreadInRadians = FMath::DegreesToRadians(Spread);

	FVector TraceDir = FMath::VRandCone(CameraRot.Vector(), SpreadInRadians);
	FVector TraceEnd = CameraLoc + (TraceDir * CurrentWeaponData.MaxRange);

	PlayWeaponEffects();
	ApplyRecoilAndCameraShake();
	ServerHandleFire(CameraLoc, TraceEnd);

	if (CurrentAmmo <= 0 || CurrentWeaponData.FireMode == EWeaponFireMode::Single)
	{
		StopFire();
		if (CurrentAmmo <= 0) Reload();
	}
}

void ABaseWeapon::ServerHandleFire_Implementation(FVector StartLocation, FVector EndLocation)
{
	if (CurrentAmmo <= 0 || bIsReloading) return;

	AController* DamageInstigator = OwnerCharacter ? OwnerCharacter->GetController() : nullptr;

	if (GetNetMode() == NM_DedicatedServer || (OwnerCharacter && !OwnerCharacter->IsLocallyControlled()))
	{
		CurrentAmmo--;
	}

	BurstCounter++;

	float DistanceSquared = FVector::DistSquared(StartLocation, EndLocation);
	float MaxRangeWithBuffer = CurrentWeaponData.MaxRange + 500.f;

	if (DistanceSquared > (MaxRangeWithBuffer * MaxRangeWithBuffer)) return;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(GetOwner());

	FVector FireDir = (EndLocation - StartLocation).GetSafeNormal();

	if (CurrentWeaponData.MaxZombiePenetration <= 1)
	{
		FHitResult Hit;
		bool bHasHit = GetWorld()->LineTraceSingleByChannel(Hit, StartLocation, EndLocation, ECC_Bullet, Params);

		if (bHasHit && Hit.GetActor())
		{
			UGameplayStatics::ApplyPointDamage(Hit.GetActor(), CurrentWeaponData.Damage, FireDir, Hit, DamageInstigator, this, UDamageType::StaticClass());
		}
	}
	else
	{
		TArray<FHitResult> Hits;
		GetWorld()->LineTraceMultiByChannel(Hits, StartLocation, EndLocation, ECC_Bullet, Params);

		int32 PenetratedZombiesCount = 0;
		TSet<AActor*> HitActorsAlready;

		for (const FHitResult& SingleHit : Hits)
		{
			if (SingleHit.GetActor() && !HitActorsAlready.Contains(SingleHit.GetActor()))
			{
				UGameplayStatics::ApplyPointDamage(SingleHit.GetActor(), CurrentWeaponData.Damage, FireDir, SingleHit, DamageInstigator, this, UDamageType::StaticClass());

				HitActorsAlready.Add(SingleHit.GetActor());
				PenetratedZombiesCount++;

				if (PenetratedZombiesCount >= CurrentWeaponData.MaxZombiePenetration) break;
				if (SingleHit.bBlockingHit) break;
			}
		}
	}
}

void ABaseWeapon::OnRep_BurstCounter()
{
	PlayWeaponEffects();
}

void ABaseWeapon::PlayWeaponEffects()
{
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

	if (HasAuthority()) ServerReload_Implementation(ReloadTimeToUse);
	else                ServerReload(ReloadTimeToUse);
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

		if (!HasAuthority()) Server_CancelReload();
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

void ABaseWeapon::ApplyRecoilAndCameraShake()
{
	if (!OwnerController) return;

	float RandomPitch = FMath::RandRange(-CurrentWeaponData.RecoilRandomness, CurrentWeaponData.RecoilRandomness);
	float RandomYaw = FMath::RandRange(-CurrentWeaponData.RecoilRandomness, CurrentWeaponData.RecoilRandomness);

	float FinalPitch = CurrentWeaponData.RecoilPitch + RandomPitch;
	float FinalYaw = CurrentWeaponData.RecoilYaw + RandomYaw;

	if (HamaComponent && HamaComponent->bIsAiming)
	{
		FinalPitch *= CurrentWeaponData.AimRecoilMultiplier;
		FinalYaw *= CurrentWeaponData.AimRecoilMultiplier;
	}

	TargetRecoilOffset.Pitch += FinalPitch;
	TargetRecoilOffset.Yaw += FinalYaw;

	SetActorTickEnabled(true);
}

void ABaseWeapon::ResetRecoil()
{
	TargetRecoilOffset = FRotator::ZeroRotator;
	CurrentRecoilOffset = FRotator::ZeroRotator;
	SetActorTickEnabled(false);
}