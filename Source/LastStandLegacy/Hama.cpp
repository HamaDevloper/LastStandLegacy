// Fill out your copyright notice in the Description page of Project Settings.

#include "Hama.h"
#include "HamaComponent.h"
#include "HamaMovementComponent.h"
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

	// ڕێگەپێدان بە کڕاکردن (Crouch)
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
	SpringArm->SetupAttachment(GetMesh());
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

// -----------------------------------------------------------------------------
// Gameplay Lifecycle (دەستپێکی کایەکە)
// -----------------------------------------------------------------------------
void AHama::BeginPlay()
{
	Super::BeginPlay();

	// وەرگرتنی پۆینتەری کۆنترۆڵەر
	OwnerController = Cast<APlayerController>(GetController());
	if (OwnerController)
	{
		// چالاککردنی سیستمی Enhanced Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(OwnerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	if (PlayerCrossHairClass)
	{
		CrossHairRef = CreateWidget<UUserWidget>(GetWorld(), PlayerCrossHairClass);
		if (CrossHairRef)
		{
			CrossHairRef->AddToViewport();
		}
	}
}

void AHama::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	if (OwnerController)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(OwnerController->GetLocalPlayer()))
		{
			Subsystem->RemoveMappingContext(DefaultMappingContext);
		}
	}
	CrossHairRef = nullptr;
}

void AHama::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
}

// Called every frame
void AHama::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// -----------------------------------------------------------------------------
// Input Binding (بەستنەوەی فەنکشنەکان بە ئینپوتەکانەوە)
// -----------------------------------------------------------------------------
void AHama::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

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
	
		EnhancedInput->BindAction(AimAction, ETriggerEvent::Started, this, &AHama::AimActionPressed);
		EnhancedInput->BindAction(AimAction, ETriggerEvent::Completed, this, &AHama::AimActionReleased);
	
		EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &AHama::FireActionPressed);
		EnhancedInput->BindAction(FireAction, ETriggerEvent::Completed, this, &AHama::FireActionReleased);
	}
}

// -----------------------------------------------------------------------------
// Input Callback Functions (لۆجیکی کارکردنی ئینپوتەکان)
// -----------------------------------------------------------------------------

void AHama::StaminaUpPerk()
{
	if (!HamaComponent) return;

	//PlayDrinkPerkAnimation(StaminaUpMontage);

	if (!HasAuthority())
	{
		// کڵایەنت تەنها داواکاری دەنێرێت و چاوەڕێی سێرڤەر دەکات
		Server_StaminaUpPerk();
	}
	else
	{
		// ئەگەر خۆی سێرڤەر بوو ڕاستەوخۆ زیادی دەکات
		HamaComponent->IncreaseMaxStamina(100.f);
	}
}

void AHama::Server_StaminaUpPerk_Implementation()
{
	HamaComponent->IncreaseMaxStamina(100.f);
}

void AHama::FireActionPressed()
{
	if (!HamaComponent) return;
	if (HamaComponent->bIsSprinting) HamaComponent->StopSprinting();
	HamaComponent->SetFiring(true);
}

void AHama::FireActionReleased()
{
	if (!HamaComponent) return;
	HamaComponent->SetFiring(false);
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
	float ApplySensitivity = bIsAiming ? AimingSensitivity : NormalSensitivity;

	AddControllerYawInput(lookVector.X * ApplySensitivity);
	AddControllerPitchInput(lookVector.Y * ApplySensitivity);
}

void AHama::JumpActionPressed()
{
	if (IsSprinting())
	{
		HamaComponent->StopSprinting();
	}
	// ئەگەر کارەکتەرەکە کڕنووشی بردبوو (Crouch)، پێش بازدانەکە با هەستێتەوە
	if (bIsCrouched) UnCrouch();

	Jump();
}

void AHama::CrouchActionPressed()
{
	if (!HamaComponent) return;
	bIsCrouchButtonHold = true;
	if (HamaComponent && IsSprinting())
	{
		HamaComponent->StopSprinting();
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

void AHama::PlayDrinkPerkAnimation(UAnimMontage* PerkMontageToPlay)
{
	// ئەگەر مۆنتاژەکە بوونی نەبوو، کۆدەکە ڕادەگرێت
	if (!PerkMontageToPlay) return;

	// ١. لێدانی ئەنیمەیشنەکە دەستبەجێ لای خۆت (0ms لاگ)
	PlayAnimMontage(PerkMontageToPlay);

	// ٢. ناردنی بۆ سێرڤەر ئەگەر کڵایەنت بوویت
	if (!HasAuthority())
	{
		Server_PlayDrinkPerkAnimation(PerkMontageToPlay); // مۆنتاژەکە دەنێرێت بۆ سێرڤەر
	}
	else
	{
		Multicast_PlayDrinkPerkAnimation(PerkMontageToPlay);
	}
}

void AHama::Server_PlayDrinkPerkAnimation_Implementation(UAnimMontage* PerkMontageToPlay)
{
	// سێرڤەر مۆنتاژەکە وەردەگرێت و دەیدات بە مۆڵتیکاست
	Multicast_PlayDrinkPerkAnimation(PerkMontageToPlay);
}

void AHama::Multicast_PlayDrinkPerkAnimation_Implementation(UAnimMontage* PerkMontageToPlay)
{
	// ڕێگری لە دووبارەبوونەوە لای خۆت
	if (IsLocallyControlled()) return;

	// ٣. کڵایەنتەکانی تر ڕێک ئەو مۆنتاژە لێدەدەنەوە کە نێردراوە
	if (PerkMontageToPlay)
	{
		PlayAnimMontage(PerkMontageToPlay);
	}
}

bool AHama::IsSprinting() const
{
	return HamaComponent && HamaComponent->IsSprinting();
}