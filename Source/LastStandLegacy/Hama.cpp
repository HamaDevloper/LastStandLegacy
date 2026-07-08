#include "Hama.h"
#include "HamaMovementComponent.h"
#include "HamaPlayerState.h"
#include "HamaMainWidget.h"
#include "LastStandLegacyGameState.h"
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
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "BasePerk.h"
#include "InteractInterface.h" 
#include "Engine/DamageEvents.h"
#include "MeleeDamageType.h"
#include "DrawDebugHelpers.h"

// -----------------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------------
AHama::AHama(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UHamaMovementComponent>(ACharacter::CharacterMovementComponentName))
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    bReplicates = true;
    SetReplicateMovement(true);
    SetNetUpdateFrequency(70.f);
    SetMinNetUpdateFrequency(33.f);

    if (GetCharacterMovement())
    {
        GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
    }

    HamaComponent = CreateDefaultSubobject<UHamaComponent>(TEXT("HamaComponent"));
    HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
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
}

const float AHama::CrossHairTimer = 0.1f;

// -----------------------------------------------------------------------------
// Gameplay Lifecycle
// -----------------------------------------------------------------------------
void AHama::BeginPlay()
{
    Super::BeginPlay();

    OwnerController = Cast<APlayerController>(GetController());

    if (IsLocallyControlled() && MainWidgetClass)
    {
        MainWidgetRef = CreateWidget<UHamaMainWidget>(GetWorld(), MainWidgetClass);
        if (MainWidgetRef)
        {
            MainWidgetRef->AddToViewport();
            MainWidgetRef->InitializeWidget(this);

            if (AHamaPlayerState* HamaPS = GetPlayerState<AHamaPlayerState>())
            {
                MainWidgetRef->UpdatePointsText(HamaPS->GetPoints());
                MainWidgetRef->UpdateKillsText(HamaPS->GetKills());
            }
            if (ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>())
            {
                MainWidgetRef->UpdateRoundText(GS->GetCurrentRound());
            }
            else
            {
                MainWidgetRef->UpdateRoundText(1);
            }
        }
    }

    GetWorldTimerManager().ClearTimer(PingUpdateTimerHandle);
    GetWorldTimerManager().SetTimer(PingUpdateTimerHandle, this, &AHama::UpdatePingUI, 1.f, true);
    GetWorld()->GetTimerManager().SetTimer(InteractTimerHandle, this, &AHama::CheckForInteractables, 0.1f, true);

    if (HasAuthority()) CreateDefaultWeapon();
    StartCrossHairTimer();
}
 
void AHama::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
    GetWorldTimerManager().ClearTimer(CrossHairTimerHandle);

    if (MainWidgetRef)
    {
        MainWidgetRef->RemoveFromParent();
        MainWidgetRef = nullptr;
    }  
}

void AHama::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AHama, CurrentWeapon);
    DOREPLIFETIME(AHama, bHasFastHands);
    DOREPLIFETIME(AHama, bHasDoubleTap);
    DOREPLIFETIME(AHama, bHasMuleKick);
    DOREPLIFETIME(AHama, PrimaryWeapon);
    DOREPLIFETIME(AHama, SecondaryWeapon);
    DOREPLIFETIME(AHama, ThirdWeapon);
    DOREPLIFETIME(AHama, bIsDead);
    DOREPLIFETIME_CONDITION(AHama, OwnedPerks, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AHama, bIsDeathMachineActive, COND_OwnerOnly);
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

    if (!OwnerController || !SnapTarget)
    {
        SetActorTickEnabled(false);
        return;
    }

    if (SnapTarget->IsDead())
    {
        SnapTarget = nullptr;
        SetActorTickEnabled(false);
        return;
    }

    USkeletalMeshComponent* TargetMesh = SnapTarget->GetMesh();
    if (!TargetMesh)
    {
        SnapTarget = nullptr;
        SetActorTickEnabled(false);
        return;
    }

    FVector CurrentCamLoc;
    FRotator CurrentCamRot;
    OwnerController->GetPlayerViewPoint(CurrentCamLoc, CurrentCamRot);

    FVector SnapLocation;
    if (TargetMesh->DoesSocketExist(SnapSocketName))
    {
        FVector SocketLoc = TargetMesh->GetSocketLocation(SnapSocketName);
        FVector ActorLoc = SnapTarget->GetActorLocation();

        SnapLocation = FVector(ActorLoc.X, ActorLoc.Y, SocketLoc.Z);
    }
    else
    {
        SnapLocation = SnapTarget->GetActorLocation();
    }

    FRotator TargetRot = (SnapLocation - CurrentCamLoc).Rotation();
    FRotator CurrentRot = OwnerController->GetControlRotation();
    FRotator NewRot = FMath::RInterpConstantTo(CurrentRot, TargetRot, DeltaTime, SnapInterpSpeed);

    OwnerController->SetControlRotation(NewRot);

    if (CurrentRot.Equals(TargetRot, 0.1f))
    {
        SnapTarget = nullptr;
        SetActorTickEnabled(false);
    }
}

void AHama::CheckForInteractables()
{
    if (!IsLocallyControlled() || !OwnerController) return;

    FVector CameraLocation;
    FRotator ViewRotation;
    OwnerController->GetPlayerViewPoint(CameraLocation, ViewRotation);

    FVector StartLocation = GetActorLocation() + FVector(0.f, 0.f, 50.f);
    FVector EndLocation = StartLocation + (ViewRotation.Vector() * SetIntractDistance);

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    Params.bReturnPhysicalMaterial = false;

    FCollisionShape Sphere = FCollisionShape::MakeSphere(45.f);

    FTraceDelegate TraceDelegate;
    TraceDelegate.BindUObject(this, &AHama::OnInteractTraceCompleted);

    GetWorld()->AsyncSweepByChannel(
        EAsyncTraceType::Single,
        StartLocation,
        EndLocation,
        FQuat::Identity,
        ECC_Intract,
        Sphere,
        Params,
        FCollisionResponseParams::DefaultResponseParam,
        &TraceDelegate
    );
}

void AHama::OnInteractTraceCompleted(
    const FTraceHandle& Handle, FTraceDatum& Datum)
{
    // ئەگەر هیچ Hit نەبوو
    if (Datum.OutHits.IsEmpty() || !Datum.OutHits[0].GetActor())
    {
        if (FocusedInteractable)
        {
            FocusedInteractable = nullptr;
            if (MainWidgetRef) MainWidgetRef->HideInteractMessage();
        }
        return;
    }

    IInteractInterface* NewFocus = Cast<IInteractInterface>(
        Datum.OutHits[0].GetActor());

    if (!NewFocus)
    {
        if (FocusedInteractable)
        {
            FocusedInteractable = nullptr;
            if (MainWidgetRef) MainWidgetRef->HideInteractMessage();
        }
        return;
    }

    if (NewFocus != FocusedInteractable)
    {
        FocusedInteractable = NewFocus;
    }

    if (MainWidgetRef)
    {
        if (FocusedInteractable->CanInteract(this))
            MainWidgetRef->ShowInteractMessage(
                FocusedInteractable->GetInteractMessage());
        else
            MainWidgetRef->HideInteractMessage();
    }
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

        if (MainWidgetRef)
        {
            MainWidgetRef->UpdateCrosshairState(bHit);
        }
    }
}

void AHama::OnPlayerStateChanged(APlayerState* NewPlayerState, APlayerState* OldPlayerState)
{
    Super::OnPlayerStateChanged(NewPlayerState, OldPlayerState);

    if (NewPlayerState)
    {
        BindPlayerStateEvents();

        if (AHamaPlayerState* HamaPS = Cast<AHamaPlayerState>(NewPlayerState))
        {
            EHamaAbilityType MyRole = HamaPS->GetAssignedRole();

            if (MyRole != EHamaAbilityType::None)
            {
                if (HamaAbilityComponent)
                {
                    HamaAbilityComponent->SetAssignedAbility(MyRole);
                }
            }
            ApplyRoleVisuals(MyRole);
        }
    }
}

void AHama::BindPlayerStateEvents()
{
    if (AHamaPlayerState* HamaPS = GetPlayerState<AHamaPlayerState>())
    {
        HamaPS->OnPointsChanged.Unbind();
        HamaPS->OnKillsChanged.Unbind();
        HamaPS->OnPointsChanged.BindUObject(this, &AHama::HandlePointsChanged);
        HamaPS->OnKillsChanged.BindUObject(this, &AHama::HandleKillsChanged);

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
        else
        {
            if (CurrentWeapon) CurrentWeapon->Destroy();
            PrimaryWeapon = SpawnedWeapon;
        }

        CurrentWeapon = SpawnedWeapon;
        CurrentWeapon->EquipWeapon(this);
        OnRep_CurrentWeapon();
        AttachWeaponToMesh(CurrentWeapon);
        OnWeaponChanged.Broadcast(CurrentWeapon);
    }
}

void AHama::GiveWeapon(TSubclassOf<ABaseWeapon> WeaponClassToGive)
{
    if (!HasAuthority() || !WeaponClassToGive) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ABaseWeapon* SpawnedWeapon = GetWorld()->SpawnActor<ABaseWeapon>(WeaponClassToGive, GetActorLocation(), GetActorRotation(), SpawnParams);

    if (SpawnedWeapon)
    {
        if (!PrimaryWeapon)
        {
            PrimaryWeapon = SpawnedWeapon;
        }
        else if (!SecondaryWeapon)
        {
            SecondaryWeapon = SpawnedWeapon;
        }
        else if (!ThirdWeapon && bHasMuleKick)
        {
            ThirdWeapon = SpawnedWeapon;
        }
        else
        {
            if (CurrentWeapon)
            {
                if (CurrentWeapon == PrimaryWeapon) PrimaryWeapon = SpawnedWeapon;
                else if (CurrentWeapon == SecondaryWeapon) SecondaryWeapon = SpawnedWeapon;
                else if (CurrentWeapon == ThirdWeapon) ThirdWeapon = SpawnedWeapon;

                ABaseWeapon* WeaponToDestroy = CurrentWeapon;
                CurrentWeapon = nullptr;
                WeaponToDestroy->Destroy();
            }
        }

        if (CurrentWeapon && CurrentWeapon != SpawnedWeapon && !CurrentWeapon->IsPendingKillPending())
        {
            CurrentWeapon->SetActorHiddenInGame(true);
            CurrentWeapon->SetActorEnableCollision(false);
        }

        CurrentWeapon = SpawnedWeapon;
        CurrentWeapon->EquipWeapon(this);
        AttachWeaponToMesh(CurrentWeapon);
        OnRep_CurrentWeapon();
        OnWeaponChanged.Broadcast(CurrentWeapon);

        // 🚀 دیبەگی سەرکەوتن: پێمان دەڵێت کە چەکەکە هاتە دەستمان و ناوی چەکەکەش دەهێنێت
        if (GEngine)
        {
            FString WeaponName = CurrentWeapon->GetName();
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("SUCCESS: %s is now in hands!"), *WeaponName));
        }
        UE_LOG(LogTemp, Warning, TEXT("GiveWeapon Success: %s equipped."), *SpawnedWeapon->GetName());
    }
    else
    {
        // 🚀 دیبەگی شکست: ئەگەر کێشەیەک هەبوو لە بلوپرێنتی چەکەکە و دروست نەبوو
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("FAILED: Weapon did not spawn!"));
        }
        UE_LOG(LogTemp, Error, TEXT("GiveWeapon Failed: Weapon spawn returned null."));
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

    if (IsLocallyControlled() && MainWidgetRef && CurrentWeapon)
    {
        CurrentWeapon->OnAmmoChanged.BindUObject(this, &AHama::HandleAmmoChanged);
        HandleAmmoChanged(CurrentWeapon->GetCurrentAmmo(), CurrentWeapon->GetReserveAmmo());
    }

    OnWeaponChanged.Broadcast(CurrentWeapon);
}

void AHama::HandleAmmoChanged(int32 CurrentAmmo, int32 ReserveAmmo)
{
    if (IsLocallyControlled() && MainWidgetRef)
    {
        if (CurrentWeapon)
        {
            MainWidgetRef->UpdateAmmoText(CurrentWeapon->GetCurrentAmmo(), CurrentWeapon->GetReserveAmmo());

            if (bIsDeathMachineActive) return;

            int32 MaxClipSize = CurrentWeapon->GetMaxClipAmmo();
            int32 LowAmmoThreshold = FMath::RoundToInt(MaxClipSize * 0.25f);

            if (CurrentWeapon->GetCurrentAmmo() == 0 && CurrentWeapon->GetReserveAmmo() <= 0)
            {
                MainWidgetRef->ShowAmmoWarning(TEXT("NoAmmo!"));
            }
            else if (CurrentWeapon->GetCurrentAmmo() <= LowAmmoThreshold && CurrentWeapon->GetCurrentAmmo() > 0)
            {
                MainWidgetRef->ShowAmmoWarning(TEXT("LOW AMMO"));
            }
            else
            {
                MainWidgetRef->HideAmmoWarning();
            }
        }
    }
}
void AHama::RefillAllWeapons()
{
    if (PrimaryWeapon) PrimaryWeapon->RefillAmmo();
    if (SecondaryWeapon) SecondaryWeapon->RefillAmmo();
    if (ThirdWeapon) ThirdWeapon->RefillAmmo();
}

void AHama::SwapWeapon()
{
    if (!SwapWeaponMontage) return;
    if (IsDrinkingPerk()) return;

    ABaseWeapon* NextWeapon = nullptr;
   
    if (CurrentWeapon == PrimaryWeapon)
    {
        if (SecondaryWeapon) NextWeapon = SecondaryWeapon;
        else if (ThirdWeapon) NextWeapon = ThirdWeapon;
    }
    else if (CurrentWeapon == SecondaryWeapon)
    {
        if (ThirdWeapon) NextWeapon = ThirdWeapon;
        else if (PrimaryWeapon) NextWeapon = PrimaryWeapon;
    }
    else if (CurrentWeapon == ThirdWeapon)
    {
        if (PrimaryWeapon) NextWeapon = PrimaryWeapon;
        else if (SecondaryWeapon) NextWeapon = SecondaryWeapon;
    }

    if (!NextWeapon || NextWeapon == CurrentWeapon) return;

    UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
    if (!AnimInstance) return;

    if (AnimInstance->Montage_IsPlaying(SwapWeaponMontage))
    {
        AnimInstance->Montage_Stop(0.1f, SwapWeaponMontage);
    }

    PendingWeaponForSwap = NextWeapon;

    float TargetPlayRate = 1.0f;

    bool bIsAdrenalineActive = false;
    if (GetWorld() && GetWorld()->GetGameState())
    {
        if (ALastStandLegacyGameState* GS = Cast<ALastStandLegacyGameState>(GetWorld()->GetGameState()))
        {
            bIsAdrenalineActive = GS->IsTeamAdrenalineActive();
        }
    }

    if (HasFastHands() || bIsAdrenalineActive)
    {
        TargetPlayRate = 2.0f;
    }

    AnimInstance->Montage_Play(SwapWeaponMontage, TargetPlayRate);

    FOnMontageEnded MontageEndedDelegate;
    MontageEndedDelegate.BindUObject(this, &AHama::OnSwapWeaponMontageEnded);
    AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, SwapWeaponMontage);

    if (IsLocallyControlled())
    {
        Server_SwapWeapon(NextWeapon);
    }
}

void AHama::Server_SwapWeapon_Implementation(ABaseWeapon* NewWeapon)
{
    if (!NewWeapon) return;

    PendingWeaponForSwap = NewWeapon;

    UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
    if (AnimInstance && SwapWeaponMontage)
    {
        if (AnimInstance->Montage_IsPlaying(SwapWeaponMontage))
        {
            AnimInstance->Montage_Stop(0.1f, SwapWeaponMontage);
        }

        float BasePlayRate = 1.0f;

        bool bIsAdrenalineActive = false;
        if (GetWorld() && GetWorld()->GetGameState())
        {
            if (ALastStandLegacyGameState* GS = Cast<ALastStandLegacyGameState>(GetWorld()->GetGameState()))
            {
                bIsAdrenalineActive = GS->IsTeamAdrenalineActive();
            }
        }

        if (HasFastHands() || bIsAdrenalineActive)
        {
            BasePlayRate = 2.0f;
        }

        float ServerPlayRate = BasePlayRate * 1.15f;

        AnimInstance->Montage_Play(SwapWeaponMontage, ServerPlayRate);

        FOnMontageEnded ServerMontageEndedDelegate;
        ServerMontageEndedDelegate.BindUObject(this, &AHama::OnSwapWeaponMontageEnded);
        AnimInstance->Montage_SetEndDelegate(ServerMontageEndedDelegate, SwapWeaponMontage);
    }
    else
    {
        CompleteWeaponSwap();
    }
}

void AHama::OnSwapWeaponMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (HasAuthority())
    {
        CompleteWeaponSwap();
    }
}

void AHama::CompleteWeaponSwap()
{
    if (!HasAuthority() || !PendingWeaponForSwap) return;

    if (CurrentWeapon)
    {
        if (CurrentWeapon->IsReloading())
        {
            CurrentWeapon->CancelReload();
        }

        CurrentWeapon->SetActorHiddenInGame(true);
        CurrentWeapon->SetActorEnableCollision(false);
    }

    CurrentWeapon = PendingWeaponForSwap;
    CurrentWeapon->SetActorHiddenInGame(false);
    CurrentWeapon->SetActorEnableCollision(true);

    CurrentWeapon->EquipWeapon(this);
    AttachWeaponToMesh(CurrentWeapon);
    OnRep_CurrentWeapon();

    OnWeaponChanged.Broadcast(CurrentWeapon);

    // بەتاڵکردنەوەی پۆینتەرەکە بۆ گۆڕینی داهاتوو
    PendingWeaponForSwap = nullptr;
}

void AHama::GiveDeathMachine(TSubclassOf<ABaseWeapon> WeaponClass, float Duration)
{
    if (!HasAuthority() || !WeaponClass) return;

    if (GetWorldTimerManager().IsTimerActive(DeathMachineTimerHandle))
    {
        GetWorldTimerManager().SetTimer(DeathMachineTimerHandle, this, &AHama::RemoveDeathMachine, Duration, false);
        return;
    }

    if (CurrentWeapon)
    {
        if (CurrentWeapon->IsReloading()) CurrentWeapon->CancelReload();
        PreDeathMachineWeapon = CurrentWeapon;
        PreDeathMachineWeapon->SetActorHiddenInGame(true);
        PreDeathMachineWeapon->SetActorEnableCollision(false);
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ActiveDeathMachine = GetWorld()->SpawnActor<ABaseWeapon>(WeaponClass, GetActorLocation(), GetActorRotation(), SpawnParams);

    if (ActiveDeathMachine)
    {
        bIsDeathMachineActive = true;
        ForceNetUpdate();

        CurrentWeapon = ActiveDeathMachine;
        CurrentWeapon->EquipWeapon(this);
        AttachWeaponToMesh(CurrentWeapon);
        OnRep_CurrentWeapon();
        OnWeaponChanged.Broadcast(CurrentWeapon);
    }
    GetWorldTimerManager().SetTimer(DeathMachineTimerHandle, this, &AHama::RemoveDeathMachine, Duration, false);
}

void AHama::RemoveDeathMachine()
{
    if (!HasAuthority()) return;

    bIsDeathMachineActive = false;
    ForceNetUpdate();

    if (ActiveDeathMachine)
    {
        ActiveDeathMachine->Destroy();
        ActiveDeathMachine = nullptr;
    }

    if (PreDeathMachineWeapon)
    {
        CurrentWeapon = PreDeathMachineWeapon;
        CurrentWeapon->SetActorHiddenInGame(false);
        CurrentWeapon->SetActorEnableCollision(true);
        CurrentWeapon->EquipWeapon(this);
        AttachWeaponToMesh(CurrentWeapon);
        OnRep_CurrentWeapon();
        OnWeaponChanged.Broadcast(CurrentWeapon);

        PreDeathMachineWeapon = nullptr;
    }
}

ABaseWeapon* AHama::GetWeaponByClass(TSubclassOf<ABaseWeapon> WeaponClassToCheck) const
{
    if (!WeaponClassToCheck) return nullptr;

    if (PrimaryWeapon && PrimaryWeapon->GetClass() == WeaponClassToCheck) return PrimaryWeapon;
    if (SecondaryWeapon && SecondaryWeapon->GetClass() == WeaponClassToCheck) return SecondaryWeapon;
    if (ThirdWeapon && ThirdWeapon->GetClass() == WeaponClassToCheck) return ThirdWeapon;

    return nullptr;
}

void AHama::RefillSpecificWeaponAmmo(TSubclassOf<ABaseWeapon> WeaponClassToRefill)
{
    if (!WeaponClassToRefill || !HasAuthority()) return;

    if (PrimaryWeapon && PrimaryWeapon->GetClass() == WeaponClassToRefill)
    {
        PrimaryWeapon->RefillAmmo();
    }
    else if (SecondaryWeapon && SecondaryWeapon->GetClass() == WeaponClassToRefill)
    {
        SecondaryWeapon->RefillAmmo();
    }
    else if (ThirdWeapon && ThirdWeapon->GetClass() == WeaponClassToRefill)
    {
        ThirdWeapon->RefillAmmo();
    }
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
        EnhancedInput->BindAction(SwapWeaponAction, ETriggerEvent::Started, this, &AHama::SwapWeapon);
        EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &AHama::InteractActionPressed);
        EnhancedInput->BindAction(GamepadXAction, ETriggerEvent::Triggered, this, &AHama::GamepadXActionPressed);
        EnhancedInput->BindAction(GamepadXAction, ETriggerEvent::Completed, this, &AHama::GamepadXActionReleased);
        EnhancedInput->BindAction(MeleeAction, ETriggerEvent::Started, this, &AHama::MeleeActionPressed);
    }
}

// -----------------------------------------------------------------------------
// Input Callback Functions
// -----------------------------------------------------------------------------
void AHama::FireActionPressed()
{
    if (!HamaComponent || !CurrentWeapon) return;
    if (IsDrinkingPerk()) return;
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
    if (IsDrinkingPerk()) return;
    bIsAimButtonHold = true;

    if (HamaComponent->IsSprinting())
    {
        HamaComponent->StopSprinting();
    }

    HamaComponent->SetAiming(true);
    OnAim(true);

    bool bIsGamepadAiming = false;

    if (OwnerController)
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(OwnerController->GetLocalPlayer()))
        {
            TArray<FKey> BoundKeys = Subsystem->QueryKeysMappedToAction(AimAction);

            for (const FKey& Key : BoundKeys)
            {
                if (Key.IsGamepadKey() && OwnerController->IsInputKeyDown(Key))
                {
                    bIsGamepadAiming = true;
                    break;
                }
            }
        }
    }

    if (bIsGamepadAiming)
    {
        AimPressedSitck();
    }
}

void AHama::AimActionReleased()
{
    if (!HamaComponent || !CurrentWeapon) return;
    bIsAimButtonHold = false;

    HamaComponent->SetAiming(false);
    OnAim(false);

    SnapTarget = nullptr;
    SetActorTickEnabled(false);
}

void AHama::AimPressedSitck()
{
    if (!OwnerController || !CurrentWeapon) return;

    FVector  StartLocation;
    FRotator StartRotation;
    OwnerController->GetPlayerViewPoint(StartLocation, StartRotation);
    FVector CameraForward = StartRotation.Vector();

    float WeaponRange = CurrentWeapon->GetWeaponMaxRange();
    float BestDotProduct = 0.99f;

    AZombie* TargetToSnap = nullptr;
    FVector  FinalSnapLocation;

    TArray<AActor*> OverlappedZombies;
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(this);

    UKismetSystemLibrary::SphereOverlapActors(
        this, StartLocation, WeaponRange,
        TArray<TEnumAsByte<EObjectTypeQuery>>(),
        AZombie::StaticClass(), ActorsToIgnore, OverlappedZombies);

    FName TargetSocket = bHasDeadshot ? FName("head") : FName("spine_04");

    for (AActor* Actor : OverlappedZombies)
    {
        AZombie* PotentialTarget = Cast<AZombie>(Actor);
        if (!PotentialTarget || PotentialTarget->IsDead()) continue;

        USkeletalMeshComponent* PotMesh = PotentialTarget->GetMesh();
        if (!PotMesh) continue;

        FVector TargetLocation = PotMesh->DoesSocketExist(TargetSocket)
            ? PotMesh->GetSocketLocation(TargetSocket)
            : PotentialTarget->GetActorLocation();

        FVector ToTargetDir = (TargetLocation - StartLocation).GetSafeNormal();
        float   CurrentDot = FVector::DotProduct(CameraForward, ToTargetDir);

        if (CurrentDot > BestDotProduct)
        {
            FHitResult           LineHit;
            FCollisionQueryParams QueryParams;
            QueryParams.AddIgnoredActor(this);
            QueryParams.AddIgnoredActor(PotentialTarget);

            if (!GetWorld()->LineTraceSingleByChannel(
                LineHit, StartLocation, TargetLocation, ECC_Visibility, QueryParams))
            {
                BestDotProduct = CurrentDot;
                TargetToSnap = PotentialTarget;
                FinalSnapLocation = TargetLocation;
            }
        }
    }

    if (TargetToSnap)
    {
        SnapTarget = TargetToSnap;
        SnapSocketName = TargetSocket;
        SetActorTickEnabled(true);
    }
}


void AHama::ReloadActionPressed()
{
    if (!CurrentWeapon || CurrentWeapon->ReserveAmmo <= 0) return;
    if (GetDeathMachine()) return;
    if (IsDrinkingPerk()) return;
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

    AddControllerYawInput(lookVector.X * ApplySensitivity);
    AddControllerPitchInput(lookVector.Y * ApplySensitivity);
}

void AHama::JumpActionPressed()
{
    if (HamaComponent && HamaComponent->IsSlide())
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
    if (HamaComponent && HamaComponent->IsSlide()) return;

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
    if (IsDrinkingPerk()) return;
    if (HamaComponent->IsAiming())
    {
        HamaComponent->SetAiming(false);
        OnAim(false);
    }
    if (HamaComponent->IsSlide()) return;
    if (bIsFireButtonHold && CurrentWeapon) CurrentWeapon->StopFire();
    if (CurrentWeapon && CurrentWeapon->bIsReloading) CurrentWeapon->CancelReload();
    if (GetCharacterMovement() && GetCharacterMovement()->IsCrouching()) UnCrouch();

    HamaComponent->StartSprinting();
}

void AHama::AbilityActionPressed()
{
    if (HamaComponent && HamaComponent->IsDowned()) return;
    if (IsDrinkingPerk()) return;
    if (!HamaAbilityComponent || !HamaAbilityComponent->IsPowerFull()) return;
    HamaAbilityComponent->Server_ActivateAbility();
}


void AHama::ApplyRoleVisuals(EHamaAbilityType NewRole)
{
    if (!GetMesh()) return;
    if (const FRoleVisualData* VisualData = RoleVisuals.Find(NewRole))
    {
        if (VisualData->RoleMesh)
        {
            GetMesh()->SetSkeletalMesh(VisualData->RoleMesh);
        }
        if (VisualData->RoleAnimBP)
        {
            GetMesh()->SetAnimInstanceClass(VisualData->RoleAnimBP);
        }
    }
}

void AHama::Server_StartPerkDrink(ABasePerk* TargetPerk)
{
    if (!TargetPerk || !HasAuthority()) return;
    if (GetWorldTimerManager().IsTimerActive(PerkDrinkTimerHandle)) return;

    PendingPerkID = TargetPerk->GetPerkID();

    Multicast_PlayDrinkPerkAnimation(TargetPerk);

    float DrinkDuration = DrinkPerkMontage ? DrinkPerkMontage->GetPlayLength() : 2.0f;

    GetWorldTimerManager().SetTimer(PerkDrinkTimerHandle, this, &AHama::GivePendingPerk, DrinkDuration, false);
}


void AHama::GivePendingPerk()
{
    if (HasAuthority())
    {
        AddPerkByID(PendingPerkID);
    }
}

void AHama::AddPerkByID(FName PerkID)
{
    if (PerkID.IsNone()) return;

    // زیادکردنی بۆ ناو لیستی پێرکەکان بۆ UI
    if (!OwnedPerks.Contains(PerkID))
    {
        OwnedPerks.Add(PerkID);
        if (HasAuthority()) ForceNetUpdate();
    }

    if (PerkID == FName(TEXT("FastHands")))
    {
        bHasFastHands = true;
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Fast Hands Perk Acquired!"));
        if (HasAuthority()) ForceNetUpdate();
    }
 
    else if (PerkID == FName(TEXT("Juggernaut")))
    {
        if (HealthComponent)
        {
            HealthComponent->UpgradeHealth(250.f);
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Juggernaut Perk Acquired! Health Increased!"));
        }
    }
   
    else if (PerkID == FName(TEXT("StaminaUp")))
    {
        if (HamaComponent)
        {
            HamaComponent->UpgradeMaxStamina(250.f);
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Stamina Up Perk Acquired! Stamina Increased!"));
        }
    }

    else if (PerkID == FName(TEXT("DoubleTap")))
    {
        bHasDoubleTap = true;
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Double Tap Perk Acquired!"));
        if (HasAuthority())  ForceNetUpdate();
    }
    else if (PerkID == FName(TEXT("Deadshot")))
    {
        bHasDeadshot = true;
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Deadshot Perk Acquired!"));
        if (HasAuthority()) ForceNetUpdate();
    }
    else if (PerkID == FName(TEXT("MuleKick")))
    {
        bHasMuleKick = true;
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Mule Kick Perk Acquired!"));
        if (HasAuthority()) ForceNetUpdate();
    }
}

void AHama::HandleDeath()
{
    OwnedPerks.Empty();
    bHasFastHands = false;
    bHasDoubleTap = false;
    bHasDeadshot = false;
    bHasMuleKick = false;
    if (HamaComponent) HamaComponent->ResetStamina();
}


void AHama::Multicast_PlayDrinkPerkAnimation_Implementation(ABasePerk* TargetPerk)
{
    if (!TargetPerk || !DrinkPerkMontage) return;

    if (CurrentWeapon)
    {
        CurrentWeapon->StopFire();
        if (CurrentWeapon->IsReloading()) CurrentWeapon->CancelReload();
    }

    if (HamaComponent)
    {
        if (HamaComponent->IsAiming())
        {
            HamaComponent->SetAiming(false);
            OnAim(false);
            SnapTarget = nullptr;
            SetActorTickEnabled(false);
        }

        if (IsSprinting())
        {
            HamaComponent->StopSprinting();
        }
    }
   

    // -------------------------------------------------------------------------

    UStaticMesh* BottleMesh = TargetPerk->GetBottleMesh();

    if (BottleMesh)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;

        CurrentSpawnedBottle = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), GetActorLocation(), GetActorRotation(), SpawnParams);
        if (CurrentSpawnedBottle && CurrentSpawnedBottle->GetStaticMeshComponent())
        {
            CurrentSpawnedBottle->GetStaticMeshComponent()->SetStaticMesh(BottleMesh);
            CurrentSpawnedBottle->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, TEXT("PerkBottleSocket"));
        }
    }

    UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
    if (AnimInstance)
    {
        AnimInstance->Montage_Play(DrinkPerkMontage, 1.0f);

        FOnMontageEnded MontageEndedDelegate;
        MontageEndedDelegate.BindUObject(this, &AHama::OnDrinkPerkAnimationCompleteFromMontage);
        AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, DrinkPerkMontage);
    }
}

void AHama::OnDrinkPerkAnimationCompleteFromMontage(UAnimMontage* Montage, bool bInterrupted)
{
    if (CurrentSpawnedBottle)
    {
        CurrentSpawnedBottle->Destroy();
        CurrentSpawnedBottle = nullptr;
    }
}

void AHama::OnRep_OwnedPerks()
{
    // ١. پشکنین دەکەین؛ ئایا دوایین پێرک کە زیادبووە چییە؟
    if (OwnedPerks.Num() > 0)
    {
        FName LatestPerk = OwnedPerks.Last();

        // ٢. لێدانی نامەیەک بۆ دڵنیایی
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("Client Received Perk: %s"), *LatestPerk.ToString()));

        // ٣. 🚀 لێرەدا دەتوانیت فەنکشنێکی ناو وەجێتەکەت (MainWidgetRef) بانگ بکەیت بۆ ئەپدیتکردنی شاشە:
        if (MainWidgetRef)
        {
            // بۆ نموونە فەنکشنێک لە ناو بلۆپرێنتی یوئای دروست دەکەیت بە ناوی AddPerkIconToHUD
            // MainWidgetRef->AddPerkIconToHUD(LatestPerk);
        }
    }
}

float AHama::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    
    if (!HasAuthority()) return 0.f;
    if (!HealthComponent) return 0.f;

    HealthComponent->GetDamage(AppliedDamage);

    if (DamageCauser)
    {
       Client_ShowDamageIndicator(DamageCauser->GetActorLocation());
    }

    return AppliedDamage;
}

void AHama::Client_ShowDamageIndicator_Implementation(FVector DamageOrigin)
{
    // ئەگەر ئەم کارەکتەرە خۆمان کۆنتڕۆڵمان نەکردبوو (وەک بینینی یاریزانێکی تر)، بوەستە
    if (!IsLocallyControlled()) return;

    FVector PlayerLocation = GetActorLocation();

 
    DamageOrigin.Z = PlayerLocation.Z;

    // ئاڕاستەی زۆمبییەکە لە کارەکتەرەکەوە
    FVector DamageDirection = (DamageOrigin - PlayerLocation).GetSafeNormal();

    // وەرگرتنی ئاڕاستەی سەیرکردنی کامێرای یاریزانەکە
    FVector Forward = GetActorForwardVector();
    FVector Right = GetActorRightVector();

    // 🚀 بیرکاری AAA بۆ دۆزینەوەی گۆشەی نێوانیان (Dot Product)
    float ForwardDot = FVector::DotProduct(Forward, DamageDirection);
    float RightDot = FVector::DotProduct(Right, DamageDirection);

    // وەرگرتنی گۆشەکە بە ڕادیان، پاشان گۆڕینی بۆ پلە (Degrees) لە نێوان -١٨٠ بۆ ١٨٠
    float AngleRads = FMath::Atan2(RightDot, ForwardDot);
    float AngleDegrees = FMath::RadiansToDegrees(AngleRads);

    OnDamageIndicatorUpdate(AngleDegrees);
}

void AHama::InteractActionPressed()
{
    if (IsDrinkingPerk()) return;

    if (FocusedInteractable && FocusedInteractable->CanInteract(this))
    {
        AActor* InteractActor = Cast<AActor>(FocusedInteractable);
        if (InteractActor)
        {
            if (AHama* DownedPlayer = Cast<AHama>(InteractActor))
            {
                bIsCurrentlyReviving = true;
                Server_BeginRevive(DownedPlayer);
            }
            else
            {
                Server_Interact(InteractActor);
            }
        }
    }
}

void AHama::InteractActionReleased()
{
    if (bIsCurrentlyReviving)
    {
        bIsCurrentlyReviving = false;
        Server_CancelRevive();
    }
}

void AHama::GamepadXActionPressed(const FInputActionInstance& Instance)
{
    if (IsDrinkingPerk()) return;
    if (!bIsxButtonHolded && Instance.GetElapsedTime() >= 0.2f)
    {
        if (FocusedInteractable && FocusedInteractable->CanInteract(this))
        {
            bIsxButtonHolded = true;
            InteractActionPressed();
        }
    }
}

void AHama::GamepadXActionReleased()
{
    if (IsDrinkingPerk()) return;

    if (bIsxButtonHolded)
    {
        bIsxButtonHolded = false;

        if (bIsCurrentlyReviving)
        {
            bIsCurrentlyReviving = false;
            Server_CancelRevive();
        }
    }
    else
    {
        ReloadActionPressed();
    }
}

void AHama::Server_Interact_Implementation(AActor* InteractTarget)
{
    if (!InteractTarget) return;

    float Distance = FVector::Dist(GetActorLocation(), InteractTarget->GetActorLocation());
    if (Distance > 300.f) return;

    IInteractInterface* Interface = Cast<IInteractInterface>(InteractTarget);
    if (Interface)
    {
        Interface->Interact(this);
    }
}

// -----------------------------------------------------------------------------
// Melee System Implementation
// -----------------------------------------------------------------------------

bool AHama::IsMeleeing() const
{
    UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
    return AnimInstance ? AnimInstance->Montage_IsPlaying(MeleeAttackMontage) : false;
}

void AHama::MeleeActionPressed()
{
    if (IsMeleeing() || IsDrinkingPerk()) return;
    if (MeleeAttackMontage)
    {
        PlayAnimMontage(MeleeAttackMontage);
    }

    Server_ExecuteMelee();
}

void AHama::Server_ExecuteMelee_Implementation()
{
    if (IsDrinkingPerk()) return;

    if (MeleeAttackMontage && !IsLocallyControlled())
        PlayAnimMontage(MeleeAttackMontage);
}

void AHama::PerformMeleeHitDetection()
{
    if (!HasAuthority()) return;

    FVector CameraLocation;
    FRotator ViewRotation;

    if (OwnerController)
    {
        OwnerController->GetPlayerViewPoint(CameraLocation, ViewRotation);
    }
    else
    {
        CameraLocation = GetActorLocation() + FVector(0.f, 0.f, 50.f);
        ViewRotation = GetActorRotation();
    }

    FVector StartLocation = GetActorLocation() + FVector(0.f, 0.f, 50.f);
    FVector EndLocation = StartLocation + (ViewRotation.Vector() * MeleeRange);

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    if (CurrentWeapon)
    {
        Params.AddIgnoredActor(CurrentWeapon);
    }

    FHitResult HitResult;
    FCollisionShape MeleeSphere = FCollisionShape::MakeSphere(MeleeRadius);

    bool bHit = GetWorld()->SweepSingleByChannel(
        HitResult,
        StartLocation,
        EndLocation,
        FQuat::Identity,
        ECC_Bullet,
        MeleeSphere,
        Params
    );

    if (bHit && HitResult.GetActor())
    {
        if(AZombie* HitZombie = Cast<AZombie>(HitResult.GetActor()))
        {
            if (HitZombie->IsDead()) return;

            AActor* HitActor = HitResult.GetActor();
            FVector ShotDirection = ViewRotation.Vector();

            FPointDamageEvent PointDamageEvent;
            PointDamageEvent.Damage = MeleeDamage;
            PointDamageEvent.HitInfo = HitResult;
            PointDamageEvent.ShotDirection = ShotDirection;
            PointDamageEvent.DamageTypeClass = UMeleeDamageType::StaticClass();

            HitActor->TakeDamage(MeleeDamage, PointDamageEvent, OwnerController, this);
        }
    }
}

bool AHama::CanInteract(AHama* InteractingPlayer)
{
    return HamaComponent && HamaComponent->IsDowned() && !bIsDead;
}

FString AHama::GetInteractMessage()
{
    return FString(TEXT("Hold [F/X] To Revive"));
}

void AHama::Interact(AHama* InteractingPlayer)
{
}

void AHama::Server_BeginRevive_Implementation(AHama* DownedPlayer)
{
    // پشکنینی ئاسایش: ئایا یاریزانەکە بوونی هەیە و بەڕاستی کەوتووە؟
    if (!DownedPlayer || !DownedPlayer->HealthComponent || !DownedPlayer->HamaComponent->IsDowned()) return;

    // ڕێگری کردن لەوەی دوو کەس لە یەک کاتدا هەڵیبستێننەوە
    if (GetWorldTimerManager().IsTimerActive(ReviveTimerHandle)) return;

    float ReviveTime = DefaultReviveTime;

    // ١. ئایا خۆم پزیشکم؟
    UHamaAbilityComponent* MyAbilityComp = FindComponentByClass<UHamaAbilityComponent>();
    if (MyAbilityComp && MyAbilityComp->GetAssignedAbility() == EHamaAbilityType::MedicalSupport)
    {
        ReviveTime /= 2.0f;
    }

    // ٢. ئایا Blitz کارایە؟
    ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>();
    if (GS && GS->IsTeamAdrenalineActive())
    {
        ReviveTime /= 2.0f;
    }

    // دەستپێکردنی تایمەر لەسەر سێرڤەر (هیچ کلایێنتێک ناتوانێت فێڵ بکات)
    FTimerDelegate ReviveDel;
    ReviveDel.BindUFunction(this, FName("Server_CompleteRevive"), DownedPlayer);

    GetWorldTimerManager().SetTimer(ReviveTimerHandle, ReviveDel, ReviveTime, false);
}

void AHama::Server_CancelRevive_Implementation()
{
    GetWorldTimerManager().ClearTimer(ReviveTimerHandle);
}

void AHama::Server_CompleteRevive(AHama* DownedPlayer)
{
    if (DownedPlayer && DownedPlayer->HealthComponent && DownedPlayer->HamaComponent->IsDowned())
    {
        DownedPlayer->HealthComponent->Revive();

        if (AHamaPlayerState* MyPS = GetPlayerState<AHamaPlayerState>())
        {
            MyPS->AddPoints(100);
        }
    }
}