#include "Hama.h"
#include "HamaMovementComponent.h"
#include "HamaPlayerState.h"
#include "HamaMainWidget.h"
#include "HamaAbilityComponent.h"
#include "Net/UnrealNetwork.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EngineUtils.h"
#include "Zombie.h"

// -----------------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------------
AHama::AHama(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UHamaMovementComponent>(ACharacter::CharacterMovementComponentName))
{
    // گرنگ: Tick لێرەدا دەکوژێنینەوە؛ لۆژیکی قورسی کۆنمان تێدا نەهێشتووە
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    bReplicates = true;
    SetReplicateMovement(true);
    SetNetUpdateFrequency(66.f);
    SetMinNetUpdateFrequency(33.f);

    if (GetCharacterMovement())
    {
        GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
    }

    HamaComponent = CreateDefaultSubobject<UHamaComponent>(TEXT("HamaComponent"));
    HamaAbilityComponent = CreateDefaultSubobject<UHamaAbilityComponent>(TEXT("HamaAbilityComponent"));
    HamaMovementComponent = Cast<UHamaMovementComponent>(GetCharacterMovement());

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength = 300.f;
    SpringArm->bUsePawnControlRotation = true;

    TPCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TPCamera"));
    TPCamera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    TPCamera->bUsePawnControlRotation = false;

    FPCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FPCamera"));
    FPCamera->SetupAttachment(GetMesh(), FName("head"));
    FPCamera->bUsePawnControlRotation = true;

    bIsStickyAiming = false;
}

const float AHama::CrossHairTimer = 0.1f;

// -----------------------------------------------------------------------------
// Gameplay Lifecycle
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

    if (IsLocallyControlled() && MainWidgetClass)
    {
        MainWidgetRef = CreateWidget<UHamaMainWidget>(GetWorld(), MainWidgetClass);
        if (MainWidgetRef)
        {
            MainWidgetRef->AddToViewport();

            if (AHamaPlayerState* HamaPS = GetPlayerState<AHamaPlayerState>())
            {
                MainWidgetRef->UpdatePointsText(HamaPS->GetPoints());
                MainWidgetRef->UpdateKillsText(HamaPS->GetKills());
            }
        }
    }

    GetWorldTimerManager().ClearTimer(PingUpdateTimerHandle);
    GetWorldTimerManager().SetTimer(PingUpdateTimerHandle, this, &AHama::UpdatePingUI, 1.f, true);

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
    DOREPLIFETIME_CONDITION(AHama, CurrentHealth, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AHama, MaxHealth, COND_OwnerOnly);
}

void AHama::Landed(const FHitResult& Hit)
{
    Super::Landed(Hit);

    if (HamaComponent)
    {
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
    StartCrossHairTimer();
}

void AHama::OnRep_Controller()
{
    Super::OnRep_Controller();
    StartCrossHairTimer();
}

void AHama::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // لەبەر ئەوەی پۆست-پڕۆسێس و خاوکردنەوەمان بردووەتە ناو لۆژیکی Look، ئۆتۆماتیکی دەیبڕینەوە بۆ نزمکردنی لۆد
    SetActorTickEnabled(false);
}

// -----------------------------------------------------------------------------
// Crosshair & Weapon Logic
// -----------------------------------------------------------------------------
void AHama::StartCrossHairTimer()
{
    if (!IsLocallyControlled()) return;
    if (GetWorldTimerManager().IsTimerActive(CrossHairTimerHandle)) return;
    GetWorldTimerManager().SetTimer(CrossHairTimerHandle, this, &AHama::CrossHairTrace, CrossHairTimer, true);
}

void AHama::CrossHairTrace()
{
    if (!CurrentWeapon || !OwnerController) return;

    FVector TraceStart;
    FRotator TraceRotation;
    OwnerController->GetPlayerViewPoint(TraceStart, TraceRotation);

    const float TraceDistance = CurrentWeapon->GetWeaponMaxRange();
    const FVector TraceEnd = TraceStart + (TraceRotation.Vector() * TraceDistance);

    FTraceDelegate TraceDelegate;
    TraceDelegate.BindUObject(this, &AHama::OnCrossHairTraceCompleted);

    FCollisionQueryParams Params(SCENE_QUERY_STAT(CrossHairTrace), false);
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(CurrentWeapon);

    GetWorld()->AsyncLineTraceByChannel(EAsyncTraceType::Single, TraceStart, TraceEnd, ECC_CrossHair, Params, FCollisionResponseParams::DefaultResponseParam, &TraceDelegate);
}

void AHama::OnCrossHairTraceCompleted(const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum)
{
    bool bHit = false;

    if (TraceDatum.OutHits.IsValidIndex(0))
    {
        AActor* HitActor = TraceDatum.OutHits[0].GetActor();
        if (IsValid(HitActor))
        {
            bHit = true;
        }
    }

    if (bHit != bLastCrossHairState)
    {
        bLastCrossHairState = bHit;
        CrossHairUpdate(bHit);
    }
}

void AHama::OnPlayerStateChanged(APlayerState* NewPlayerState, APlayerState* OldPlayerState)
{
    Super::OnPlayerStateChanged(NewPlayerState, OldPlayerState);

    // ئەم فەنکشنە تەنها کاتێک کار دەکات کە PlayerState بە تەواوی گەیشتبێتە کڕایەنت
    if (NewPlayerState)
    {
        BindPlayerStateEvents();
    }
}

void AHama::BindPlayerStateEvents()
{
    // وەرگرتنی PlayerState بە شێوازی پارێزراو
    if (AHamaPlayerState* HamaPS = GetPlayerState<AHamaPlayerState>())
    {
        // لادانی بەستنەوەی کۆن ئەگەر هەبێت (بۆ ڕێگری لە دووبارەبوونەوە)
        HamaPS->OnPointsChanged.RemoveDynamic(this, &AHama::HandlePointsChanged);
        HamaPS->OnKillsChanged.RemoveDynamic(this, &AHama::HandleKillsChanged);

        // بەستنەوەی دیسپاچەرەکانی C++ بە فەنکشنەکانی کارەکتەرەوە
        HamaPS->OnPointsChanged.AddDynamic(this, &AHama::HandlePointsChanged);
        HamaPS->OnKillsChanged.AddDynamic(this, &AHama::HandleKillsChanged);

        if (IsLocallyControlled() && MainWidgetRef)
        {
            MainWidgetRef->UpdatePointsText(HamaPS->GetPoints());
            MainWidgetRef->UpdateKillsText(HamaPS->GetKills());
        }
    }
}

void AHama::HandlePointsChanged(int32 NewPoints)
{
    if (IsLocallyControlled() && MainWidgetRef)
    {
        MainWidgetRef->UpdatePointsText(NewPoints);
    }
}

void AHama::HandleKillsChanged(int32 NewKills)
{
    if (IsLocallyControlled() && MainWidgetRef)
    {
        MainWidgetRef->UpdateKillsText(NewKills);
    }
}

void AHama::UpdatePingUI()
{
    if (APlayerState* PS = GetPlayerState())
    {
        float ExactPing = PS->GetPingInMilliseconds();
        OnUIUpdatePing(FMath::RoundToInt(ExactPing));
    }
}

void AHama::CreateDefaultWeapon()
{
    if (!DefaultWeapon) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ABaseWeapon* SpawnedWeapon = GetWorld()->SpawnActor<ABaseWeapon>(DefaultWeapon, GetActorLocation(), GetActorRotation(), SpawnParams);

    if (SpawnedWeapon)
    {
        if (!PrimaryWeapon) PrimaryWeapon = SpawnedWeapon;
        else if (!SecondaryWeapon) SecondaryWeapon = SpawnedWeapon;
        else if (!ThirdWeapon) ThirdWeapon = SpawnedWeapon;
        else
        {
            if (CurrentWeapon) CurrentWeapon->Destroy();
            PrimaryWeapon = SpawnedWeapon;
        }

        CurrentWeapon = SpawnedWeapon;
        CurrentWeapon->EquipWeapon(this);
        AttachWeaponToMesh(CurrentWeapon);
        OnWeaponChanged.Broadcast(CurrentWeapon);
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
    OnWeaponChanged.Broadcast(CurrentWeapon);
}

// -----------------------------------------------------------------------------
// Input Binding
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
        EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHama::Move);
        EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHama::Look);
        EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &AHama::JumpActionPressed);
        EnhancedInput->BindAction(SprintAction, ETriggerEvent::Started, this, &AHama::SprintActionPressed);
        EnhancedInput->BindAction(SwitchCameraAction, ETriggerEvent::Triggered, this, &AHama::SwitchCameraPressed);
        EnhancedInput->BindAction(SwitchCameraAction, ETriggerEvent::Completed, this, &AHama::SwitchCameraReleased);
        EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Started, this, &AHama::CrouchActionPressed);
        EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Completed, this, &AHama::CrouchActionReleased);
        EnhancedInput->BindAction(AimAction, ETriggerEvent::Started, this, &AHama::AimActionPressed);
        EnhancedInput->BindAction(AimAction, ETriggerEvent::Completed, this, &AHama::AimActionReleased);
        EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &AHama::FireActionPressed);
        EnhancedInput->BindAction(FireAction, ETriggerEvent::Completed, this, &AHama::FireActionReleased);
        EnhancedInput->BindAction(ReloadAction, ETriggerEvent::Started, this, &AHama::ReloadActionPressed);
        EnhancedInput->BindAction(AbilityAction, ETriggerEvent::Started, this, &AHama::AbilityActionPressed);
    }
}

void AHama::OnRep_Health() {}

// -----------------------------------------------------------------------------
// Input Callback Functions
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
    CurrentWeapon->StopFire();
}

void AHama::AimActionPressed()
{
    if (!HamaComponent || !CurrentWeapon) return;
    bIsAimButtonHold = true;

    if (HamaComponent->bIsSprinting)
    {
        HamaComponent->StopSprinting();
    }

    HamaComponent->SetAiming(true);
    OnAim(true);

    // یەکەم ڕاکێشان (Snap) لە کاتی داگرتنی دوگمەکە بەپێی مەودای ڕاستەقینە
    AimPressedSitck();
}

void AHama::AimActionReleased()
{
    if (!HamaComponent || !CurrentWeapon) return;
    bIsAimButtonHold = false;

    bIsStickyAiming = false;
    LockedTarget = nullptr;

    HamaComponent->SetAiming(false);
    OnAim(false);
}

void AHama::AimPressedSitck()
{
    if (!OwnerController || !CurrentWeapon) return;

    FVector StartLocation;
    FRotator StartRotation;
    OwnerController->GetPlayerViewPoint(StartLocation, StartRotation);
    FVector CameraForward = StartRotation.Vector();

    float WeaponRange = CurrentWeapon->GetWeaponMaxRange();
    float BestDotProduct = 0.90f; // سنوری جێگیر بۆ ڕاکێشانی نیشانە

    AZombie* TargetToSnap = nullptr;

    TArray<AActor*> OverlappedZombies;
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(this);

    // بەکارهێنانی سستەمی ئۆڤەرلاپ لەبری TActorIterator بۆ کارایی بەرز
    UKismetSystemLibrary::SphereOverlapActors(
        this, StartLocation, WeaponRange,
        TArray<TEnumAsByte<EObjectTypeQuery>>(),
        AZombie::StaticClass(), ActorsToIgnore, OverlappedZombies
    );

    for (AActor* Actor : OverlappedZombies)
    {
        AZombie* PotentialTarget = Cast<AZombie>(Actor);
        if (!PotentialTarget || PotentialTarget->bIsDead) continue;

        FVector TargetLocation = PotentialTarget->GetActorLocation();
        if (PotentialTarget->GetMesh() && PotentialTarget->GetMesh()->DoesSocketExist(FName("spine_04")))
        {
            TargetLocation = PotentialTarget->GetMesh()->GetSocketLocation(FName("spine_04"));
        }

        FVector ToTargetDir = (TargetLocation - StartLocation).GetSafeNormal();
        float CurrentDot = FVector::DotProduct(CameraForward, ToTargetDir);

        if (CurrentDot > BestDotProduct)
        {
            FHitResult LineHit;
            FCollisionQueryParams QueryParams;
            QueryParams.AddIgnoredActor(this);
            QueryParams.AddIgnoredActor(PotentialTarget);

            if (!GetWorld()->LineTraceSingleByChannel(LineHit, StartLocation, TargetLocation, ECC_Visibility, QueryParams))
            {
                BestDotProduct = CurrentDot;
                TargetToSnap = PotentialTarget;
            }
        }
    }

    if (TargetToSnap)
    {
        LockedTarget = TargetToSnap;
        bIsStickyAiming = true;

        FVector TargetLocation = TargetToSnap->GetMesh()->GetSocketLocation(FName("spine_04"));
        FRotator DesiredRot = (TargetLocation - StartLocation).Rotation();

        OwnerController->SetControlRotation(DesiredRot);
    }
}

void AHama::ReloadActionPressed()
{
    if (!CurrentWeapon || CurrentWeapon->ReserveAmmo <= 0) return;

    if (HamaComponent && HamaComponent->IsSprinting())
    {
        HamaComponent->StopSprinting();
    }

    CurrentWeapon->StopFire();
    CurrentWeapon->Reload();
}

void AHama::Move(const FInputActionValue& Value)
{
    if (!OwnerController) return;
    FVector2D MovementVector = Value.Get<FVector2D>();
    FRotator ControllRotation = OwnerController->GetControlRotation();
    FRotator YawRotation(0.f, ControllRotation.Yaw, 0.f);

    FVector MoveForward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
    FVector MoveRight = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

    AddMovementInput(MoveForward, MovementVector.Y);
    AddMovementInput(MoveRight, MovementVector.X);
}

void AHama::Look(const FInputActionValue& Value)
{
    if (!OwnerController) return;

    FVector2D lookVector = Value.Get<FVector2D>();
    float ApplySensitivity = bIsAimButtonHold ? AimingSensitivity : NormalSensitivity;

    // پشکنین و بەکارهێنانی TWeakObjectPtr بە شێوەیەکی سەلامەت بۆ ڕێگری لە کڕاش
    if (bIsStickyAiming && LockedTarget.IsValid())
    {
        if (lookVector.Length() > 0.7f)
        {
            bIsStickyAiming = false;
            LockedTarget = nullptr;
        }
    }
    else if (bIsStickyAiming && !LockedTarget.IsValid())
    {
        bIsStickyAiming = false;
        LockedTarget = nullptr;
    }

    // لۆژیکی BO3 Aim Friction Bubble (خاوکردنەوەی نەرمی نیشانە لە دەوری زۆمبی)
    if (bIsAimButtonHold && CurrentWeapon)
    {
        FVector TraceStart;
        FRotator TraceRotation;
        OwnerController->GetPlayerViewPoint(TraceStart, TraceRotation);

        float AssistDistance = CurrentWeapon->GetWeaponMaxRange() * 0.8f;
        FVector TraceEnd = TraceStart + (TraceRotation.Vector() * AssistDistance);

        FHitResult HitResult;
        FCollisionQueryParams Params;
        Params.AddIgnoredActor(this);

        // بڵقێک بە نیوەتیرەی ٤٥ یەکە دروست دەکەین بۆ گرتنەوەی دەوروبەری زۆمبییەکە پێش گەیشتنی فیشەک
        FCollisionShape SphereShape = FCollisionShape::MakeSphere(45.f);

        if (GetWorld()->SweepSingleByChannel(HitResult, TraceStart, TraceEnd, FQuat::Identity, ECC_Pawn, SphereShape, Params))
        {
            if (Cast<AZombie>(HitResult.GetActor()))
            {
                ApplySensitivity *= StickySlowdownMultiplier;
            }
        }
    }

    AddControllerYawInput(lookVector.X * ApplySensitivity);
    AddControllerPitchInput(lookVector.Y * ApplySensitivity);
}

void AHama::JumpActionPressed()
{
    if (HamaComponent && HamaComponent->bIsSlide)
    {
        bCanJumpSlide = true;
        StopSlideRoutine();
    }

    if (HamaComponent && HamaComponent->IsSprinting())
    {
        HamaComponent->StopSprinting();
    }

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
        if (HamaMovementComponent->IsCrouching()) UnCrouch();
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

    if (SlideMontage)
    {
        StopAnimMontage(SlideMontage);
    }
}

void AHama::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (Montage == SlideMontage && HamaComponent)
    {
        HamaComponent->StopSlide();
    }
}

void AHama::SwitchCameraPressed(const FInputActionInstance& Instance)
{
    if (Instance.GetElapsedTime() >= 0.5f)
    {
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
    if (!bIsHoldedTrigger)
    {
        bIsInRightShoulderView = !bIsInRightShoulderView;
        if (!bIsInFirstPerson)
        {
            Switchcamera(bIsInRightShoulderView);
        }
    }
    bIsHoldedTrigger = false;
}

void AHama::SprintActionPressed()
{
    if (!HamaComponent || HamaComponent->GetStamina() < 15.f || GetCharacterMovement()->IsFalling()) return;

    if (HamaComponent->bIsAiming)
    {
        HamaComponent->SetAiming(false);
        OnAim(false);
    }
    if (bIsFireButtonHold && CurrentWeapon) CurrentWeapon->StopFire();
    if (CurrentWeapon && CurrentWeapon->bIsReloading) CurrentWeapon->CancelReload();
    if (GetCharacterMovement() && GetCharacterMovement()->IsCrouching()) UnCrouch();

    HamaComponent->StartSprinting();
}

void AHama::AbilityActionPressed()
{
    if(HamaAbilityComponent) HamaAbilityComponent->Server_ActivateAbility();
}