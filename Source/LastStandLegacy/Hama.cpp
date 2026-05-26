// Fill out your copyright notice in the Description page of Project Settings.
#include "Hama.h"
#include "HamaComponent.h"
#include "HamaMovementComponent.h"
#include "BaseWeapon.h"
#include "Net/UnrealNetwork.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Blueprint/UserWidget.h"

// -----------------------------------------------------------------------------
// Constructor (سازێنەری ئەکتەرەکە)
// -----------------------------------------------------------------------------
AHama::AHama(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UHamaMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// ناچالاککردنی Tick بۆ زیادکردنی خێرایی پڕۆژەکە
	PrimaryActorTick.bCanEverTick = false;

	// ڕێکخستنەکانی نێتوۆرک و ڕێپلیکەیشن (Networking & Replication)
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(66.f);
	SetMinNetUpdateFrequency(33.f);

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	}

	// -----------------------------------------------------------------------------
	// Create & Bind Components (دروستکردن و بەستنەوەی پێکهاتەکان)
	// -----------------------------------------------------------------------------

	// دروستکردنی پێکهاتەی تایبەتی Hama
	HamaComponent = CreateDefaultSubobject<UHamaComponent>(TEXT("HamaComponent"));

	// بەستنەوەی گۆڕاوەکە بەو بزوێنەری جوڵەیەی کە لە سەرەوە (SetDefaultSubobjectClass) دیاریمان کردووە
	HamaMovementComponent = Cast<UHamaMovementComponent>(GetCharacterMovement());

	// دروستکردنی SpringArm (قۆڵی کامێرا)
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 300.f;
	SpringArm->bUsePawnControlRotation = true;

	// دروستکردنی کامێرای کەسی سێیەم (Third Person Camera)
	TPCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TPCamera"));
	TPCamera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	TPCamera->bUsePawnControlRotation = false;

	// دروستکردنی کامێرای کەسی یەکەم (First Person Camera)
	FPCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FPCamera"));
	FPCamera->SetupAttachment(GetMesh(), FName("head"));
	FPCamera->bUsePawnControlRotation = true;
}

const float AHama::CrossHairTimer = 0.1f;
// -----------------------------------------------------------------------------
// Gameplay Lifecycle (دەستپێکی کایەکە)
// -----------------------------------------------------------------------------
void AHama::BeginPlay()
{
	Super::BeginPlay();

	OwnerController = Cast<APlayerController>(GetController());

	if (IsLocallyControlled() && PlayerCrossHairClass)
	{
		CrossHairRef = CreateWidget<UUserWidget>(GetWorld(), PlayerCrossHairClass);

		if (CrossHairRef)
		{
			CrossHairRef->AddToViewport();
		}
	}

	if (HasAuthority()) CreateDefaultWeapon();
	StartCrossHairTimer();
}

void AHama::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	GetWorldTimerManager().ClearTimer(CrossHairTimerHandle);

	if (CrossHairRef)
	{
		CrossHairRef->RemoveFromParent();
		CrossHairRef = nullptr;
	}
}

void AHama::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHama, CurrentWeapon);
}

void AHama::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (HamaComponent)
	{
		// بەکارهێنانی StartSlideRoutine بۆ ئەوەی دڵنیا بین کە Delegateـەکە دەبەسترێتەوە
		if (bIsCrouchButtonHold && bCanJumpSlide)
		{
			StartSlideRoutine();
		}
	}
	bCanJumpSlide = false;
}

void AHama::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// سێرڤەر کۆنترۆڵەرەکەی وەرگرت
	StartCrossHairTimer();
}

void AHama::OnRep_Controller()
{
	Super::OnRep_Controller();

	// کڵایەنت کۆنترۆڵەرەکەی وەرگرت
	StartCrossHairTimer();
}

// Called every frame
void AHama::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AHama::StartCrossHairTimer()
{
	if (!IsLocallyControlled())
		return;

	if (!OwnerController)
	{
		OwnerController = Cast<APlayerController>(GetController());
	}

	if (!OwnerController)
		return;

	if (GetWorldTimerManager().IsTimerActive(CrossHairTimerHandle))
		return;

	GetWorldTimerManager().SetTimer(
		CrossHairTimerHandle,
		this,
		&AHama::CrossHairTrace,
		CrossHairTimer,
		true
	);
}

void AHama::CrossHairTrace()
{
	if (!CurrentWeapon) return;
	if (!OwnerController)
	{
		OwnerController = Cast<APlayerController>(GetController());
		if (!OwnerController) return;
	}

	FVector TraceStart;
	FRotator TraceRotation;

	OwnerController->GetPlayerViewPoint(TraceStart, TraceRotation);

	const FVector TraceEnd =
		TraceStart + (TraceRotation.Vector() * CurrentWeapon->MaxRange);

	FTraceDelegate TraceDelegate;
	TraceDelegate.BindUObject(this, &AHama::OnCrossHairTraceCompleted);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CrossHairTrace), false);
	Params.AddIgnoredActor(this);

	GetWorld()->AsyncLineTraceByChannel(
		EAsyncTraceType::Single,
		TraceStart,
		TraceEnd,
		ECC_Zombie,
		Params,
		FCollisionResponseParams::DefaultResponseParam,
		&TraceDelegate
	);
}

void AHama::OnCrossHairTraceCompleted(
	const FTraceHandle& TraceHandle,
	FTraceDatum& TraceDatum)
{
	const bool bHit = TraceDatum.OutHits.IsValidIndex(0) &&
		IsValid(TraceDatum.OutHits[0].GetActor());

	if (bHit != bLastCrossHairState)
	{
		bLastCrossHairState = bHit;
		CrossHairUpdate(bHit);
	}
}

void AHama::CreateDefaultWeapon()
{
	// ١. پشکنین بۆ ئەوەی دڵنیا بینەوە جۆری چەکی سەرەتایی دیاری کراوە
	if (!DefaultWeapon) return;
	// ٢. ڕێکخستنی پارامیتەرەکانی سپۆنکردن بۆ سێرڤەر
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	FVector SpawnLocation = GetActorLocation();
	FRotator SpawnRotation = GetActorRotation();

	// ٣. سپۆنکردنی چەکەکە
	ABaseWeapon* SpawnedWeapon = GetWorld()->SpawnActor<ABaseWeapon>(DefaultWeapon, SpawnLocation, SpawnRotation, SpawnParams);

	if (SpawnedWeapon)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("چەکەکە بە سەرکەوتوویی سپۆن کرا!"));
		// ٤. لۆجیکی دابەشکردنی چەکەکە بەسەر خانەکانی ئینڤێنتۆری (١، ٢، ٣)
		if (!PrimaryWeapon)
		{
			PrimaryWeapon = SpawnedWeapon;
			UE_LOG(LogTemp, Log, TEXT("چەکی یەکەم پڕکرایەوە (PrimaryWeapon)"));
		}
		else if (!SecondaryWeapon)
		{
			SecondaryWeapon = SpawnedWeapon;
			UE_LOG(LogTemp, Log, TEXT("چەکی دووەم پڕکرایەوە (SecondaryWeapon)"));
		}
		else if (!ThirdWeapon)
		{
			ThirdWeapon = SpawnedWeapon;
			UE_LOG(LogTemp, Log, TEXT("چەکی سێیەم پڕکرایەوە (ThirdWeapon)"));
		}
		else
		{
			// ئەگەر هەموو خانەکان پڕ بوون، چەکە کۆنەکە دەسڕێتەوە و ئەمە جێگەی دەگرێتەوە (یان دەتوانیت لۆجیکی فڕێدان دابنێیت)
			if (CurrentWeapon)
			{
				CurrentWeapon->Destroy();
			}
			PrimaryWeapon = SpawnedWeapon;
		}

		// ٥. دیاریکردنی چەکی دەستی ئێستای یاریزانەکە
		CurrentWeapon = SpawnedWeapon;

		FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, true);
		CurrentWeapon->AttachToComponent(GetMesh(), AttachRules, SocketName);
	}
}

void AHama::AttachWeaponToMesh(ABaseWeapon* WeaponToAttach)
{
	if (WeaponToAttach && GetMesh())
	{
		FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, true);
		WeaponToAttach->AttachToComponent(GetMesh(), AttachRules, SocketName);
	}
}

void AHama::OnRep_CurrentWeapon()
{
	AttachWeaponToMesh(CurrentWeapon);
}


// -----------------------------------------------------------------------------
// Input Binding (بەستنەوەی فەنکشنەکان بە ئینپوتەکانەوە)
// -----------------------------------------------------------------------------
void AHama::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	if (UEnhancedInputComponent* EnhancedInput = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// بەستنەوەی جوڵە و تەماشاکردن
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHama::Move);
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHama::Look);

		// بەستنەوەی بازدان
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &AHama::JumpActionPressed);
		EnhancedInput->BindAction(SprintAction, ETriggerEvent::Started, this, &AHama::SprintActionPressed);
		// بەستنەوەی گۆڕینی کامێرا (Triggered بۆ هۆڵد و Completed بۆ لادانی دەست)
		EnhancedInput->BindAction(SwitchCameraAction, ETriggerEvent::Triggered, this, &AHama::SwitchCameraPressed);
		EnhancedInput->BindAction(SwitchCameraAction, ETriggerEvent::Completed, this, &AHama::SwitchCameraReleased);

		// بەستنەوەی کڕاکردن (Crouch)
		EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Started, this, &AHama::CrouchActionPressed);
		EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Completed, this, &AHama::CrouchActionReleased);
		EnhancedInput->BindAction(AimAction, ETriggerEvent::Started, this, &AHama::AimActionPressed);
		EnhancedInput->BindAction(AimAction, ETriggerEvent::Completed, this, &AHama::AimActionReleased);
	
		EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &AHama::FireActionPressed);
		EnhancedInput->BindAction(FireAction, ETriggerEvent::Completed, this, &AHama::FireActionReleased);
		
		EnhancedInput->BindAction(ReloadAction, ETriggerEvent::Started, this, &AHama::ReloadActionPressed);
	}
}

// -----------------------------------------------------------------------------
// Input Callback Functions (لۆجیکی کارکردنی ئینپوتەکان)
// -----------------------------------------------------------------------------
void AHama::FireActionPressed()
{
	if (!HamaComponent || !CurrentWeapon) return;
	bIsFireButtonHold = true;
	CurrentWeapon->StartFire();
}

void AHama::FireActionReleased()
{
	if (!HamaComponent || !CurrentWeapon) return;
	bIsFireButtonHold = false;
	HamaComponent->SetFiring(false);
	CurrentWeapon->StopFire();
}

void AHama::AimActionPressed()
{
	if (!HamaComponent) return;
	bIsAimButtonHold = true;
	if (HamaComponent->bIsSprinting) HamaComponent->StopSprinting();
	HamaComponent->SetAiming(true);
	OnAim(true);
}

void AHama::AimActionReleased()
{
	if (!HamaComponent) return;
	bIsAimButtonHold = false;
	HamaComponent->SetAiming(false);
	OnAim(false);
}

void AHama::ReloadActionPressed()
{
	if (!CurrentWeapon) return;
	if (CurrentWeapon->ReserveAmmo <= 0) return;
	HamaComponent->SetFiring(false);
	CurrentWeapon->StopFire();
	CurrentWeapon->Reload();
}

void AHama::Move(const FInputActionValue& Value)
{
	if (!OwnerController) return;

	// وەرگرتنی داتای جوڵە (X, Y)
	FVector2D MovementVector = Value.Get<FVector2D>();

	// دۆزینەوەی ئاڕاستەی ڕووانینی کامێرا
	FRotator ControllRotation = OwnerController->GetControlRotation();
	FRotator YawRotation(0.f, ControllRotation.Yaw, 0.f);

	// حیسابکردنی ئاڕاستەی پێشەوە و تەنیشتەکان
	FVector MoveForward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	FVector MoveRight = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	// جێبەجێکردنی جوڵەکە
	AddMovementInput(MoveForward, MovementVector.Y);
	AddMovementInput(MoveRight, MovementVector.X);
}

void AHama::Look(const FInputActionValue& Value)
{
	if (!OwnerController) return;

	FVector2D lookVector = Value.Get<FVector2D>();

	// گۆڕینی هەستیاری (Sensitivity) بەپێی ئەوەی نیشانەی گرتووە یان نا
	float ApplySensitivity = bIsAimButtonHold ? AimingSensitivity : NormalSensitivity;

	AddControllerYawInput(lookVector.X * ApplySensitivity);
	AddControllerPitchInput(lookVector.Y * ApplySensitivity);
}

void AHama::JumpActionPressed()
{
	// ئەگەر لە سلایددا بوو، سلایدەکە دەوەستێنین و ڕێگە دەدەین دوای بازدانەکە سلاید بکاتەوە
	if (HamaComponent && HamaComponent->bIsSlide)
	{
		bCanJumpSlide = true;
		StopSlideRoutine(); // وەستاندنی تەواوەتی سلاید و ئەنیمەیشنەکە
	}

	if (HamaComponent && HamaComponent->IsSprinting())
	{
		HamaComponent->StopSprinting();
	}

	// ئەگەر کارەکتەرەکە کڕنووشی بردبوو، پێش بازدانەکە با هەستێتەوە
	if (GetCharacterMovement()->IsCrouching()) UnCrouch();

	Jump();
}

void AHama::CrouchActionPressed()
{
	bIsCrouchButtonHold = true;
	if (HamaComponent && HamaComponent->bIsSlide) return;

	if (HamaComponent && IsSprinting())
	{
		StartSlideRoutine();
		return;
	}

	if (GetCharacterMovement()->IsFalling()) return;

	if (HamaMovementComponent)
	{
		if(HamaMovementComponent->IsCrouching())
		{
			UnCrouch();
		}
		else Crouch();
	}
}

void AHama::CrouchActionReleased()
{
	bIsCrouchButtonHold = false;
}

void AHama::StartSlideRoutine()
{
	if (!HamaComponent) return;

	HamaComponent->StopSprinting();
	PlayAnimMontage(SlideMontage);
	HamaComponent->StartSlide();

	// بەستنەوەی فەنکشنی کۆتایی مۆنتاژ بۆ ئەوەی لە کاتی خۆی سلایدەکە بوەستێت
	if (GetMesh() && GetMesh()->GetAnimInstance() && SlideMontage)
	{
		FOnMontageEnded MontageEndedDelegate;
		MontageEndedDelegate.BindUObject(this, &AHama::OnMontageEnded);
		GetMesh()->GetAnimInstance()->Montage_SetEndDelegate(MontageEndedDelegate, SlideMontage);
	}
}

void AHama::StopSlideRoutine()
{
	if (!HamaComponent) return;

	HamaComponent->StopSlide();

	// وەستاندنی ئەنیمەیشنەکە بە زۆر (Force Stop) بۆ Slide Cancel
	if (SlideMontage)
	{
		StopAnimMontage(SlideMontage);
	}
}

void AHama::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage == SlideMontage)
	{
		if (HamaComponent)
		{
			HamaComponent->StopSlide();
		}
	}
}


void AHama::SwitchCameraPressed(const FInputActionInstance& Instance)
{
	// ئەگەر پلەی hold گەیشتە 0.5 چرکە
	if (Instance.GetElapsedTime() >= 0.5f)
	{
		// تەنها یەک جار switch بکا
		if (!bIsHoldedTrigger)
		{
			bIsHoldedTrigger = true;

			bIsInFirstPerson = !bIsInFirstPerson;

			if (TPCamera && FPCamera)
			{
				TPCamera->SetActive(!bIsInFirstPerson);
				FPCamera->SetActive(bIsInFirstPerson);
			}
		}
	}
}

void AHama::SwitchCameraReleased()
{
	// کاتێک دەستی لادەبات: ئەگەر کاتەکەی نەگەیشتبووە 0.5 چرکە (واتە هۆڵد نەبووە، تەنها وەک Tap)
	if (!bIsHoldedTrigger)
	{
		bIsInRightShoulderView = !bIsInRightShoulderView;

		if (!bIsInFirstPerson)
		{
			Switchcamera(bIsInRightShoulderView);
		}
	}

	// هەمیشە لە کاتی لادانی دەستدا ئەم گۆڕاوە ڕیست دەبێتەوە بۆ جاری داهاتوو
	bIsHoldedTrigger = false;
}

void AHama::SprintActionPressed()
{
	if (!HamaComponent) return;
	if (HamaComponent->GetStamina() < 15.f) return;
	if (HamaComponent->bIsAiming)
	{
		HamaComponent->SetAiming(false);
		OnAim(false);
	}
	if (GetCharacterMovement() && GetCharacterMovement()->IsCrouching())
	{
		UnCrouch();
	}
	HamaComponent->StartSprinting();
}

bool AHama::IsSprinting() const
{
	return HamaComponent && HamaComponent->IsSprinting();
}