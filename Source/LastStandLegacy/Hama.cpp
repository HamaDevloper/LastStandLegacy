#include "Hama.h"
#include "HamaMovementComponent.h"
#include "HamaPlayerState.h"
#include "HamaAnimInstance.h"
#include "LastStandLegacyGameState.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EngineUtils.h"
#include "Zombie.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "BasePerk.h"
#include "InteractInterface.h" 
#include "Engine/DamageEvents.h"
#include "MeleeDamageType.h"
#include "Components/SphereComponent.h"
#include "ZombieDirectorSubsystem.h"
#include "DrawDebugHelpers.h"

// -----------------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------------
AHama::AHama(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UHamaMovementComponent>(ACharacter::CharacterMovementComponentName))
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    bReplicates = true;
    SetReplicateMovement(true);
    SetNetUpdateFrequency(80.f);
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
    SpringArm->SetupAttachment(GetMesh());
    SpringArm->TargetArmLength = 300.f;
    SpringArm->bUsePawnControlRotation = true;

    TPCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TPCamera"));
    TPCamera->SetupAttachment(SpringArm);
    TPCamera->bUsePawnControlRotation = false;

    FPCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FPCamera"));
    FPCamera->SetupAttachment(GetMesh(), FName("head"));
    FPCamera->bUsePawnControlRotation = true;

    InteractSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractSphere"));
    InteractSphere->SetupAttachment(RootComponent);
    InteractSphere->SetSphereRadius(250.f);
    InteractSphere->SetCollisionProfileName(TEXT("Trigger"));
    InteractSphere->SetGenerateOverlapEvents(true);

    PerkBottleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PerkBottleMesh"));
    PerkBottleMesh->SetupAttachment(GetMesh(), TEXT("PerkBottleSocket"));
    PerkBottleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PerkBottleMesh->SetVisibility(false);
}

const float AHama::CrossHairTimer = 0.05f;

// -----------------------------------------------------------------------------
// Gameplay Lifecycle
// -----------------------------------------------------------------------------
void AHama::BeginPlay()
{
    Super::BeginPlay();

    OwnerController = Cast<APlayerController>(GetController());

    if (IsLocallyControlled())
    {
        InteractSphere->OnComponentBeginOverlap.AddDynamic(this, &AHama::OnInteractSphereBeginOverlap);
        InteractSphere->OnComponentEndOverlap.AddDynamic(this, &AHama::OnInteractSphereEndOverlap);
    }

    if (HasAuthority())
    {
        if (UWorld* World = GetWorld())
        {
            if (UZombieDirectorSubsystem* Director = World->GetSubsystem<UZombieDirectorSubsystem>())
            {
                Director->RegisterPlayer(this);
            }
        }

        CreateDefaultWeapon();
    }
    StartCrossHairTimer();
}

void AHama::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
    GetWorldTimerManager().ClearTimer(CrossHairTimerHandle);

    if (HasAuthority())
    {
        if (UWorld* World = GetWorld())
        {
            if (UZombieDirectorSubsystem* Director = World->GetSubsystem<UZombieDirectorSubsystem>())
            {
                Director->UnregisterPlayer(this);
            }
        }
    }
}

void AHama::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Params;
    Params.bIsPushBased = true;

    Params.Condition = COND_None;
    DOREPLIFETIME_WITH_PARAMS_FAST(AHama, CurrentWeapon, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AHama, bHasFastHands, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AHama, bIsDead, Params);

    Params.Condition = COND_OwnerOnly;
    DOREPLIFETIME_WITH_PARAMS_FAST(AHama, bHasDoubleTap, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AHama, bHasMuleKick, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AHama, bHasDeadshot, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AHama, bHasQuickRevive, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AHama, PrimaryWeapon, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AHama, SecondaryWeapon, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AHama, ThirdWeapon, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AHama, OwnedPerks, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AHama, bIsDeathMachineActive, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AHama, CurrentlyUpgradingWeaponClass, Params);
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

    if (!IsLocallyControlled()) return;

    if (!OwnerController || !SnapTarget)
    {
        return;
    }

    if (SnapTarget->IsDead())
    {
        SnapTarget = nullptr;
        return;
    }

    USkeletalMeshComponent* TargetMesh = SnapTarget->GetMesh();
    if (!TargetMesh)
    {
        SnapTarget = nullptr;
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
    }
}

void AHama::OnInteractSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (OtherActor && OtherActor != this && OtherActor->Implements<UInteractInterface>())
    {
        NearbyInteractablesCount++;
        if (NearbyInteractablesCount >= 1)
        {
            GetWorld()->GetTimerManager().SetTimer(InteractTimerHandle, this, &AHama::CheckForInteractables, 0.1f, true);
        }
    }
}

void AHama::OnInteractSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (OtherActor && OtherActor != this && OtherActor->Implements<UInteractInterface>())
    {
        NearbyInteractablesCount--;
        if (NearbyInteractablesCount <= 0)
        {
            NearbyInteractablesCount = 0;
            GetWorld()->GetTimerManager().ClearTimer(InteractTimerHandle);

            if (FocusedInteractable)
            {
                FocusedInteractable = nullptr;
            }

            OnInteractUpdateEvent.ExecuteIfBound(TEXT(""));
        }
    }
}

void AHama::CheckForInteractables()
{
    if (!IsLocallyControlled() || !OwnerController) return;
    if (HamaComponent && HamaComponent->IsDowned()) return;

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

void AHama::OnInteractTraceCompleted(const FTraceHandle& Handle, FTraceDatum& Datum)
{
    if (NearbyInteractablesCount <= 0)
    {
        if (FocusedInteractable)
        {
            FocusedInteractable = nullptr;
            OnInteractUpdateEvent.ExecuteIfBound(TEXT(""));
        }

        return;
    }

    if (Datum.OutHits.IsEmpty() || !Datum.OutHits[0].GetActor())
    {
        if (FocusedInteractable)
        {
            FocusedInteractable = nullptr;
            OnInteractUpdateEvent.ExecuteIfBound(TEXT(""));
        }

        return;
    }

    IInteractInterface* NewFocus = Cast<IInteractInterface>(Datum.OutHits[0].GetActor());

    if (!NewFocus)
    {
        if (FocusedInteractable)
        {
            FocusedInteractable = nullptr;
            OnInteractUpdateEvent.ExecuteIfBound(TEXT(""));
        }

        return;
    }

    if (NewFocus != FocusedInteractable)
    {
        FocusedInteractable = NewFocus;
    }

    if (FocusedInteractable->CanInteract(this))
    {
        OnInteractUpdateEvent.ExecuteIfBound(FocusedInteractable->GetInteractMessage(this));
    }
    else
    {
        OnInteractUpdateEvent.ExecuteIfBound(TEXT(""));
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
            if (IDamageableInterface* Damageable = Cast<IDamageableInterface>(HitActor))
            {
                bHit = Damageable->CanReceiveWeaponDamage();
            }
        }
    }

    if (bHit != bLastCrossHairState)
    {
        bLastCrossHairState = bHit;
        OnCrosshairUpdateEvent.ExecuteIfBound(bHit);
    }
}

void AHama::OnPlayerStateChanged(APlayerState* NewPlayerState, APlayerState* OldPlayerState)
{
    Super::OnPlayerStateChanged(NewPlayerState, OldPlayerState);

    if (NewPlayerState)
    {
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
        if (!PrimaryWeapon)
        {
            PrimaryWeapon = SpawnedWeapon;
        }
        else if (!SecondaryWeapon)
        {
            SecondaryWeapon = SpawnedWeapon;
        }
        else
        {
            if (CurrentWeapon == SecondaryWeapon) SecondaryWeapon = nullptr;
            else if (CurrentWeapon == ThirdWeapon) ThirdWeapon = nullptr;

            if (CurrentWeapon) CurrentWeapon->Destroy();
            PrimaryWeapon = SpawnedWeapon;
        }

        CurrentWeapon = SpawnedWeapon;
        MARK_PROPERTY_DIRTY_FROM_NAME(AHama, CurrentWeapon, this);

        CurrentWeapon->EquipWeapon(this);
        OnRep_CurrentWeapon();
    }
}

TArray<TSubclassOf<ABaseWeapon>> AHama::GetOwnedWeaponClasses() const
{
    TArray<TSubclassOf<ABaseWeapon>> OwnedClasses;
    OwnedClasses.Reserve(3);

    if (IsValid(PrimaryWeapon))   OwnedClasses.Add(PrimaryWeapon->GetClass());
    if (IsValid(SecondaryWeapon)) OwnedClasses.Add(SecondaryWeapon->GetClass());
    if (IsValid(ThirdWeapon))     OwnedClasses.Add(ThirdWeapon->GetClass());

    return OwnedClasses;
}

void AHama::GiveWeapon(TSubclassOf<ABaseWeapon> WeaponClassToGive)
{
    if (!HasAuthority() || !WeaponClassToGive) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ABaseWeapon* OldWeapon = CurrentWeapon;

    ABaseWeapon* SpawnedWeapon = GetWorld()->SpawnActor<ABaseWeapon>(WeaponClassToGive, GetActorLocation(), GetActorRotation(), SpawnParams);
    if (!SpawnedWeapon) return;

    if (!PrimaryWeapon)
    {
        PrimaryWeapon = SpawnedWeapon;
        MARK_PROPERTY_DIRTY_FROM_NAME(AHama, PrimaryWeapon, this);
    }
    else if (!SecondaryWeapon)
    {
        SecondaryWeapon = SpawnedWeapon;
        MARK_PROPERTY_DIRTY_FROM_NAME(AHama, SecondaryWeapon, this);
    }
    else if (!ThirdWeapon && bHasMuleKick)
    {
        ThirdWeapon = SpawnedWeapon;
        MARK_PROPERTY_DIRTY_FROM_NAME(AHama, ThirdWeapon, this);
    }
    else
    {
        ABaseWeapon* WeaponToDestroy = CurrentWeapon;

        if (CurrentWeapon == PrimaryWeapon)
        {
            PrimaryWeapon = SpawnedWeapon;
            MARK_PROPERTY_DIRTY_FROM_NAME(AHama, PrimaryWeapon, this);
        }
        else if (CurrentWeapon == SecondaryWeapon)
        {
            SecondaryWeapon = SpawnedWeapon;
            MARK_PROPERTY_DIRTY_FROM_NAME(AHama, SecondaryWeapon, this);
        }
        else if (CurrentWeapon == ThirdWeapon)
        {
            ThirdWeapon = SpawnedWeapon;
            MARK_PROPERTY_DIRTY_FROM_NAME(AHama, ThirdWeapon, this);
        }
        else
        {
            WeaponToDestroy = PrimaryWeapon;
            PrimaryWeapon = SpawnedWeapon;
            MARK_PROPERTY_DIRTY_FROM_NAME(AHama, PrimaryWeapon, this);
        }

        if (IsValid(WeaponToDestroy))
        {
            WeaponToDestroy->Destroy();
        }
    }

    if (IsValid(CurrentWeapon) && CurrentWeapon != SpawnedWeapon)
    {
        CurrentWeapon->SetActorHiddenInGame(true);
        CurrentWeapon->SetActorEnableCollision(false);
    }

    CurrentWeapon = SpawnedWeapon;
    MARK_PROPERTY_DIRTY_FROM_NAME(AHama, CurrentWeapon, this);

    CurrentWeapon->EquipWeapon(this);
    AttachWeaponToMesh(CurrentWeapon);
    OnRep_CurrentWeapon(OldWeapon);
}

void AHama::AttachWeaponToMesh(ABaseWeapon* WeaponToAttach)
{
    if (WeaponToAttach && GetMesh())
    {
        FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, true);
        WeaponToAttach->AttachToComponent(GetMesh(), AttachRules, SocketName);
    }
}

void AHama::OnRep_CurrentWeapon(ABaseWeapon* PreviousWeapon)
{
    if (PreviousWeapon)
    {
        PreviousWeapon->OnAmmoChanged.Unbind();
    }

    if (!CurrentWeapon) return;

    AttachWeaponToMesh(CurrentWeapon);

    if (IsLocallyControlled())
    {
        CurrentWeapon->OnAmmoChanged.Unbind();
        CurrentWeapon->OnAmmoChanged.BindUObject(this, &AHama::HandleAmmoChanged);
        HandleAmmoChanged(CurrentWeapon->GetCurrentAmmo(), CurrentWeapon->GetReserveAmmo());
    }
}

void AHama::HandleAmmoChanged(int32 CurrentAmmo, int32 ReserveAmmo)
{
    if (IsLocallyControlled())
    {
        OnAmmoUpdateEvent.ExecuteIfBound(CurrentAmmo, ReserveAmmo);
    }
}

void AHama::RefillAllWeapons()
{
    if (!HasAuthority()) return;

    if (PrimaryWeapon) PrimaryWeapon->RefillAmmo();
    if (SecondaryWeapon) SecondaryWeapon->RefillAmmo();
    if (ThirdWeapon) ThirdWeapon->RefillAmmo();
}

ABaseWeapon* AHama::GetNextWeaponWithAmmo() const
{
    TArray<TObjectPtr<ABaseWeapon>> SearchOrder;

    if (CurrentWeapon == PrimaryWeapon)
    {
        SearchOrder = { SecondaryWeapon, ThirdWeapon };
    }
    else if (CurrentWeapon == SecondaryWeapon)
    {
        SearchOrder = { ThirdWeapon, PrimaryWeapon };
    }
    else if (CurrentWeapon == ThirdWeapon)
    {
        SearchOrder = { PrimaryWeapon, SecondaryWeapon };
    }

    for (ABaseWeapon* Weapon : SearchOrder)
    {
        if (Weapon && Weapon->HasAmmo())
        {
            return Weapon;
        }
    }

    return nullptr;
}

void AHama::RemoveCurrentWeapon()
{
    if (!HasAuthority() || !IsValid(CurrentWeapon)) return;

    ABaseWeapon* WeaponToDestroy = CurrentWeapon;

    if (CurrentWeapon == PrimaryWeapon)
    {
        PrimaryWeapon = nullptr;
        MARK_PROPERTY_DIRTY_FROM_NAME(AHama, PrimaryWeapon, this);
    }
    else if (CurrentWeapon == SecondaryWeapon)
    {
        SecondaryWeapon = nullptr;
        MARK_PROPERTY_DIRTY_FROM_NAME(AHama, SecondaryWeapon, this);
    }
    else if (CurrentWeapon == ThirdWeapon)
    {
        ThirdWeapon = nullptr;
        MARK_PROPERTY_DIRTY_FROM_NAME(AHama, ThirdWeapon, this);
    }

    CurrentWeapon = GetNextWeaponWithAmmo();
    if (!CurrentWeapon)
    {
        if (PrimaryWeapon) CurrentWeapon = PrimaryWeapon;
        else if (SecondaryWeapon) CurrentWeapon = SecondaryWeapon;
        else if (ThirdWeapon) CurrentWeapon = ThirdWeapon;
    }

    MARK_PROPERTY_DIRTY_FROM_NAME(AHama, CurrentWeapon, this);

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (URecoilComponent* RecoilComp = PC->FindComponentByClass<URecoilComponent>())
        {
            RecoilComp->ResetRecoil();
        }
    }

    WeaponToDestroy->Destroy();

    if (CurrentWeapon)
    {
        CurrentWeapon->SetActorHiddenInGame(false);
        CurrentWeapon->SetActorEnableCollision(true);
        CurrentWeapon->EquipWeapon(this);
        AttachWeaponToMesh(CurrentWeapon);
    }

    OnRep_CurrentWeapon();
}

void AHama::AutoSwapToAvailableWeapon()
{
    if (!IsLocallyControlled()) return;

    ABaseWeapon* WeaponWithAmmo = GetNextWeaponWithAmmo();
    if (WeaponWithAmmo)
    {
        SwapWeapon(WeaponWithAmmo);
    }
}

void AHama::Input_SwapWeapon()
{
    SwapWeapon(nullptr);
}

void AHama::SwapWeapon(ABaseWeapon* TargetWeapon)
{
    if (!SwapWeaponMontage || !CurrentWeapon) return;
    if (IsDrinkingPerk()) return;
    if (HamaComponent && HamaComponent->IsDowned()) return;
    if (PendingWeaponForSwap != nullptr) return;

    if(IsSprinting())
    {
        StopSprint();
    }

    ABaseWeapon* NextWeapon = TargetWeapon;
  
    if (!NextWeapon)
    {
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
    }

    if (!NextWeapon || NextWeapon == CurrentWeapon) return;

    UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
    if (!AnimInstance) return;

    if (CurrentWeapon->IsReloading())
    {
        CurrentWeapon->CancelReload();
    }

    if (AnimInstance->Montage_IsPlaying(SwapWeaponMontage))
    {
        AnimInstance->Montage_Stop(0.1f, SwapWeaponMontage);
    }

    PendingWeaponForSwap = NextWeapon;

    float TargetPlayRate = 1.0f;
    if (ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>())
    {
        if (HasFastHands() || GS->IsTeamAdrenalineActive())
        {
            TargetPlayRate = 2.0f;
        }
    }

    AnimInstance->Montage_Play(SwapWeaponMontage, TargetPlayRate);

    FOnMontageEnded MontageEndedDelegate;
    MontageEndedDelegate.BindUObject(this, &AHama::OnSwapWeaponMontageEnded);
    AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, SwapWeaponMontage);

    if (!HasAuthority() && IsLocallyControlled())
    {
        Server_SwapWeapon(NextWeapon);
    }
}

void AHama::Server_SwapWeapon_Implementation(ABaseWeapon* NewWeapon)
{
    if (!NewWeapon) return;
    if (HamaComponent && HamaComponent->IsDowned()) return;

    if (NewWeapon != PrimaryWeapon && NewWeapon != SecondaryWeapon && NewWeapon != ThirdWeapon)
    {
        return;
    }

    PendingWeaponForSwap = NewWeapon;

    if (SwapWeaponMontage)
    {
        UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
        if (AnimInstance && AnimInstance->Montage_IsPlaying(SwapWeaponMontage))
        {
            AnimInstance->Montage_Stop(0.1f, SwapWeaponMontage);
        }

        float BasePlayRate = 1.0f;
        bool bIsAdrenalineActive = false;

        if (ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>())
        {
            bIsAdrenalineActive = GS->IsTeamAdrenalineActive();
        }

        if (HasFastHands() || bIsAdrenalineActive)
        {
            BasePlayRate = 2.0f;
        }

        PlayAnimMontage(SwapWeaponMontage, BasePlayRate);
        Multicast_PlaySwapMontage(BasePlayRate);

        if (AnimInstance)
        {
            FOnMontageEnded ServerMontageEndedDelegate;
            ServerMontageEndedDelegate.BindUObject(this, &AHama::OnSwapWeaponMontageEnded);
            AnimInstance->Montage_SetEndDelegate(ServerMontageEndedDelegate, SwapWeaponMontage);
        }
    }
    else
    {
        CompleteWeaponSwap();
    }
}

void AHama::Multicast_PlaySwapMontage_Implementation(float PlayRate)
{
    if (IsLocallyControlled()) return;

    if (SwapWeaponMontage)
    {
        PlayAnimMontage(SwapWeaponMontage, PlayRate);
    }
}

void AHama::OnSwapWeaponMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (HasAuthority())
    {
        CompleteWeaponSwap();
    }

    if (IsLocallyControlled() && !HasAuthority())
    {
        if (!bInterrupted && PendingWeaponForSwap && PendingWeaponForSwap->CanReload())
        {
            PendingWeaponForSwap->Reload();
        }
        PendingWeaponForSwap = nullptr;
    }
}

void AHama::CompleteWeaponSwap()
{
    if (!HasAuthority() || !PendingWeaponForSwap) return;

    ABaseWeapon* OldWeapon = CurrentWeapon;

    if (CurrentWeapon)
    {
        CurrentWeapon->SetActorHiddenInGame(true);
        CurrentWeapon->SetActorEnableCollision(false);
    }

    CurrentWeapon = PendingWeaponForSwap;
    MARK_PROPERTY_DIRTY_FROM_NAME(AHama, CurrentWeapon, this);

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (URecoilComponent* RecoilComp = PC->FindComponentByClass<URecoilComponent>())
        {
            RecoilComp->ResetRecoil();
        }
    }

    CurrentWeapon->SetActorHiddenInGame(false);
    CurrentWeapon->SetActorEnableCollision(true);
    CurrentWeapon->EquipWeapon(this);

    OnRep_CurrentWeapon(OldWeapon);

    ABaseWeapon* NewlyEquippedWeapon = PendingWeaponForSwap;
    PendingWeaponForSwap = nullptr;

    if (NewlyEquippedWeapon->NeedsAmmo() && NewlyEquippedWeapon->CanReload())
    {
        NewlyEquippedWeapon->Reload();
    }
}

void AHama::GiveDeathMachine(TSubclassOf<ABaseWeapon> WeaponClass, float Duration)
{
    if (!HasAuthority() || !WeaponClass) return;
    if (HamaComponent && HamaComponent->IsDowned()) return;

    if (GetWorldTimerManager().IsTimerActive(DeathMachineTimerHandle))
    {
        GetWorldTimerManager().SetTimer(DeathMachineTimerHandle, this, &AHama::RemoveDeathMachine, Duration, false);
        return;
    }

    if (CurrentWeapon)
    {
        CurrentWeapon->StopFire();

        if (CurrentWeapon->IsReloading())
        {
            CurrentWeapon->CancelReload();
        }

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
        MARK_PROPERTY_DIRTY_FROM_NAME(AHama, bIsDeathMachineActive, this);

        CurrentWeapon = ActiveDeathMachine;
        MARK_PROPERTY_DIRTY_FROM_NAME(AHama, CurrentWeapon, this);

        CurrentWeapon->EquipWeapon(this);
        AttachWeaponToMesh(CurrentWeapon);
        OnRep_CurrentWeapon();
    }

    GetWorldTimerManager().SetTimer(DeathMachineTimerHandle, this, &AHama::RemoveDeathMachine, Duration, false);
}

void AHama::RemoveDeathMachine()
{
    if (!HasAuthority()) return;

    ABaseWeapon* OldWeapon = CurrentWeapon;

    bIsDeathMachineActive = false;
    MARK_PROPERTY_DIRTY_FROM_NAME(AHama, bIsDeathMachineActive, this);

    if (IsValid(ActiveDeathMachine))
    {
        ActiveDeathMachine->StopFire();
        ActiveDeathMachine->Destroy();
        ActiveDeathMachine = nullptr;
    }

    ABaseWeapon* WeaponToEquip = nullptr;

    if (IsValid(PreDeathMachineWeapon))
    {
        WeaponToEquip = PreDeathMachineWeapon;
        PreDeathMachineWeapon = nullptr;
    }
    else
    {
        if (IsValid(PrimaryWeapon)) WeaponToEquip = PrimaryWeapon;
        else if (IsValid(SecondaryWeapon)) WeaponToEquip = SecondaryWeapon;
        else if (IsValid(ThirdWeapon)) WeaponToEquip = ThirdWeapon;
    }

    if (WeaponToEquip)
    {
        CurrentWeapon = WeaponToEquip;
        MARK_PROPERTY_DIRTY_FROM_NAME(AHama, CurrentWeapon, this);

        CurrentWeapon->SetActorHiddenInGame(false);
        CurrentWeapon->SetActorEnableCollision(true);
        CurrentWeapon->EquipWeapon(this);
        AttachWeaponToMesh(CurrentWeapon);

        OnRep_CurrentWeapon(OldWeapon);
      
        if (CurrentWeapon->CanReload())
        {
            CurrentWeapon->Reload();
        }
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

ABaseWeapon* AHama::GetWeaponOrUpgradedInstance(TSubclassOf<ABaseWeapon> TargetWeaponClass)
{
    if (!TargetWeaponClass) return nullptr;

    TSubclassOf<ABaseWeapon> UpgradedClass = nullptr;

    ABaseWeapon* CDO = TargetWeaponClass.GetDefaultObject();

    if (CDO && CDO->WeaponDataTable && !CDO->WeaponRowName.IsNone())
    {
        FWeaponData* RowData = CDO->WeaponDataTable->FindRow<FWeaponData>(CDO->WeaponRowName, TEXT("WeaponUpgradeCheck"));
        if (RowData)
        {
            UpgradedClass = RowData->UpgradedWeaponClass;
        }
    }

    TArray<TObjectPtr<ABaseWeapon>> PlayerWeapons = { PrimaryWeapon, SecondaryWeapon, ThirdWeapon };

    for (TObjectPtr<ABaseWeapon> Wep : PlayerWeapons)
    {
        if (IsValid(Wep))
        {
            if (Wep->IsA(TargetWeaponClass) || (UpgradedClass && Wep->IsA(UpgradedClass)))
            {
                return Wep;
            }
        }
    }

    return nullptr;
}

void AHama::SetCurrentlyUpgradingWeaponClass(TSubclassOf<ABaseWeapon> InWeaponClass)
{
    if (!HasAuthority()) return;

    CurrentlyUpgradingWeaponClass = InWeaponClass;
    MARK_PROPERTY_DIRTY_FROM_NAME(AHama, CurrentlyUpgradingWeaponClass, this);
}

bool AHama::IsWeaponCurrentlyUpgrading(TSubclassOf<ABaseWeapon> WeaponClassToCheck) const
{
    if (!WeaponClassToCheck) return false;

    if (CurrentlyUpgradingWeaponClass == WeaponClassToCheck)
    {
        return true;
    }

    return false;
}

// -----------------------------------------------------------------------------
// Input Binding
// -----------------------------------------------------------------------------
void AHama::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EnhancedInput = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
    {
        EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHama::Move);
        EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHama::Look);
        EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &AHama::JumpActionPressed);
        EnhancedInput->BindAction(SprintAction, ETriggerEvent::Started, this, &AHama::SprintActionPressed);
        EnhancedInput->BindAction(SwitchCameraAction, ETriggerEvent::Triggered, this, &AHama::SwitchCameraPressed);
        EnhancedInput->BindAction(SwitchCameraAction, ETriggerEvent::Completed, this, &AHama::SwitchCameraReleased);
        EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Started, this, &AHama::CrouchActionPressed);
        EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Triggered, this, &AHama::CrouchActionPressed);
        EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Completed, this, &AHama::CrouchActionReleased);
        EnhancedInput->BindAction(AimAction, ETriggerEvent::Started, this, &AHama::AimActionPressed);
        EnhancedInput->BindAction(AimAction, ETriggerEvent::Completed, this, &AHama::AimActionReleased);
        EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &AHama::FireActionPressed);
        EnhancedInput->BindAction(FireAction, ETriggerEvent::Completed, this, &AHama::FireActionReleased);
        EnhancedInput->BindAction(ReloadAction, ETriggerEvent::Started, this, &AHama::ReloadActionPressed);
        EnhancedInput->BindAction(AbilityAction, ETriggerEvent::Started, this, &AHama::AbilityActionPressed);
        EnhancedInput->BindAction(SwapWeaponAction, ETriggerEvent::Started, this, &AHama::Input_SwapWeapon);
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

    if (HamaComponent->IsSprinting())
    {
        HamaComponent->StopSprinting();
    }

    bIsAimButtonHold = true;
    HamaComponent->SetAiming(true);

    if (!HamaComponent->IsDowned())
    {
        OnAim(true);
    }

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
    //SetActorTickEnabled(false);
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

void AHama::ResetValuesAfterSprint()
{
    if (bIsAimButtonHold)
    {
        HamaComponent->SetAiming(true);
        OnAim(true);
    }
    if (CurrentWeapon)
    {
        if (bIsFireButtonHold)
        {
           CurrentWeapon->StartFire();
        }
        if (CurrentWeapon->CanReload())
        {
            CurrentWeapon->Reload();
        }
    }
   
}

void AHama::ReloadActionPressed()
{
    if (!CurrentWeapon || !CurrentWeapon->ShouldReload()) return;
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
    if (HamaComponent)
    {
        if (HamaComponent->IsDowned()) return;
        if (HamaComponent->IsDiving()) return;
        if (HamaComponent->IsSlide())
        {
            bCanJumpSlide = true;
            StopSlideRoutine();
        }
        if (HamaComponent->IsSprinting()) HamaComponent->StopSprinting();
    }

    if (GetCharacterMovement()->IsCrouching()) UnCrouch();
    Jump();
}

void AHama::CrouchActionPressed(const FInputActionInstance& Instance)
{
    bIsCrouchButtonHold = true;

    if (GetCharacterMovement()->IsFalling()) return;

    if (!HamaComponent || HamaComponent->IsSlide() || HamaComponent->IsDowned() || HamaComponent->IsDiving())
    {
        return;
    }

    if (IsSprinting())
    {
        if (Instance.GetElapsedTime() >= 0.25f)
        {
            bHasPerformedDive = true;
            StartDiving();
        }

        return;
    }

    if (Instance.GetTriggerEvent() == ETriggerEvent::Started)
    {
        if (HamaMovementComponent)
        {
            if (HamaMovementComponent->IsCrouching())
            {
                UnCrouch();
            }
            else
            {
                Crouch();
            }
        }
    }
}

void AHama::CrouchActionReleased(const FInputActionInstance& Instance)
{
    bIsCrouchButtonHold = false;
    bHasPerformedDive = false;

    if (!HamaComponent) return;

    if (IsSprinting() && Instance.GetElapsedTime() < 0.25f && !HamaComponent->IsDiving())
    {
        StartSlideRoutine();
    }
}

void AHama::StartSlideRoutine()
{
    if (!HamaComponent) return;
    if (IsSprinting())  HamaComponent->StopSprinting();
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

void AHama::StartDiving()
{
    if (!HamaComponent) return;
    if (IsSprinting())  HamaComponent->StopSprinting();
    PlayAnimMontage(DiveMontage);
    HamaComponent->StartDive();

    if (GetMesh() && GetMesh()->GetAnimInstance() && DiveMontage)
    {
        FOnMontageEnded MontageEndedDelegate;
        MontageEndedDelegate.BindUObject(this, &AHama::OnDiveMontageEnded);
        GetMesh()->GetAnimInstance()->Montage_SetEndDelegate(MontageEndedDelegate, DiveMontage);
    }
}

void AHama::StopDiving()
{
    if (!HamaComponent) return;
    HamaComponent->StopDive();

    if (DiveMontage)
    {
        StopAnimMontage(DiveMontage);
    }
}

void AHama::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (Montage == SlideMontage && HamaComponent)
    {
        HamaComponent->StopSlide();
    }
}

void AHama::OnDiveMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (Montage == DiveMontage && HamaComponent)
    {
        HamaComponent->StopDive();
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

bool AHama::IsMovingForward() const
{
    const FVector InputVector = GetLastMovementInputVector();

    const float MinSprintSpeedSq = 550.f * 550.f;
    if (InputVector.IsNearlyZero() || GetVelocity().SizeSquared() < MinSprintSpeedSq)
    {
        return false;
    }

    const FVector Forward2D = GetActorForwardVector().GetSafeNormal2D();
    const FVector Input2D = InputVector.GetSafeNormal2D();

    const float ForwardDot = FVector::DotProduct(Forward2D, Input2D);

    return ForwardDot > 0.5f;
}

void AHama::SprintActionPressed()
{
    if (!IsMovingForward()) return;
    if (!HamaComponent) return;
    if (IsDrinkingPerk()) return;
    if (IsSliding()) return;
    if (IsDiving()) return;
    if (GetCharacterMovement()->IsFalling()) return;

    if (HamaComponent->IsAiming())
    {
        HamaComponent->SetAiming(false);
        OnAim(false);
    }
    if (bIsFireButtonHold && CurrentWeapon) CurrentWeapon->StopFire();
    if (CurrentWeapon && CurrentWeapon->bIsReloading) CurrentWeapon->CancelReload();
    if (GetCharacterMovement() && GetCharacterMovement()->IsCrouching()) UnCrouch();

    HamaComponent->StartSprinting();
}

void AHama::StopSprint()
{
    if (!HamaComponent) return;

    HamaComponent->StopSprinting();
}

void AHama::OnSprintStopped()
{
    if (!CurrentWeapon) return;

    if (IsAimButtonHold())
    {
        OnAim(true);
    }

    if (bIsFireButtonHold)
    {
        FireActionPressed();
    }
   
    if (CurrentWeapon->CanReload())
    {
        CurrentWeapon->Reload();
    }
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
        if (!VisualData->RoleMesh.IsNull())
        {
            if (USkeletalMesh* LoadedMesh = VisualData->RoleMesh.LoadSynchronous())
            {
                GetMesh()->SetSkeletalMesh(LoadedMesh);
            }
        }

        if (!VisualData->RoleAnimBP.IsNull())
        {
            if (UClass* LoadedAnimClass = VisualData->RoleAnimBP.LoadSynchronous())
            {
                GetMesh()->SetAnimInstanceClass(LoadedAnimClass);
            }
        }
    }
}

void AHama::Server_StartPerkDrink(ABasePerk* TargetPerk)
{
    if (!TargetPerk || !HasAuthority() || IsDowned() || bIsDead) return;
    if (GetWorldTimerManager().IsTimerActive(PerkDrinkTimerHandle)) return;

    PendingPerkID = TargetPerk->GetPerkID();

    Multicast_PlayDrinkPerkAnimation(TargetPerk);

    float DrinkDuration = DrinkPerkMontage ? DrinkPerkMontage->GetPlayLength() : 2.0f;

    GetWorldTimerManager().SetTimer(PerkDrinkTimerHandle, this, &AHama::GivePendingPerk, DrinkDuration, false);
}


void AHama::GivePendingPerk()
{
    if (!HasAuthority() || IsDowned() || bIsDead)
    {
        PendingPerkID = NAME_None;
        return;
    }

    if (!PendingPerkID.IsNone())
    {
        AddPerkByID(PendingPerkID);
        PendingPerkID = NAME_None;
    }
}

void AHama::AddPerkByID(FName PerkID)
{
    if (PerkID.IsNone() || !HasAuthority()) return;

    if (!OwnedPerks.Contains(PerkID))
    {
        OwnedPerks.Add(PerkID);
        MARK_PROPERTY_DIRTY_FROM_NAME(AHama, OwnedPerks, this);

        if (IsLocallyControlled())
        {
            OnPerksChangedEvent.ExecuteIfBound(OwnedPerks);
        }
    }

    if (PerkID == FName(TEXT("FastHands")))
    {
        bHasFastHands = true;
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Fast Hands Perk Acquired!"));
        MARK_PROPERTY_DIRTY_FROM_NAME(AHama, bHasFastHands, this);
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
        }

        if (!IsLocallyControlled())
        {
            Client_OnStaminUpAcquired(250.f);
        }
    }

    else if (PerkID == FName(TEXT("DoubleTap")))
    {
        bHasDoubleTap = true;
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Double Tap Perk Acquired!"));
        MARK_PROPERTY_DIRTY_FROM_NAME(AHama, bHasDoubleTap, this);
    }
    else if (PerkID == FName(TEXT("Deadshot")))
    {
        bHasDeadshot = true;
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Deadshot Perk Acquired!"));
        MARK_PROPERTY_DIRTY_FROM_NAME(AHama, bHasDeadshot, this);
    }
    else if (PerkID == FName(TEXT("MuleKick")))
    {
        bHasMuleKick = true;
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Mule Kick Perk Acquired!"));
        MARK_PROPERTY_DIRTY_FROM_NAME(AHama, bHasMuleKick, this);
    }
    else if (PerkID == FName(TEXT("QuickRevive")))
    {
        bHasQuickRevive = true;
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("PhD Flopper Perk Acquired!"));
        MARK_PROPERTY_DIRTY_FROM_NAME(AHama, bHasQuickRevive, this);
    }
}

void AHama::HandleDeath()
{
    if (!HasAuthority()) return;

    GetWorldTimerManager().ClearTimer(PerkDrinkTimerHandle);
    PendingPerkID = NAME_None;

    OwnedPerks.Empty();
    MARK_PROPERTY_DIRTY_FROM_NAME(AHama, OwnedPerks, this);

    bHasFastHands = false;
    bHasDoubleTap = false;
    bHasDeadshot = false;
    bHasMuleKick = false;
    bHasQuickRevive = false;

    MARK_PROPERTY_DIRTY_FROM_NAME(AHama, bHasFastHands, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AHama, bHasDoubleTap, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AHama, bHasDeadshot, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AHama, bHasMuleKick, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AHama, bHasQuickRevive, this);

    if (HamaComponent)
    {
        HamaComponent->ResetStamina();
    }
    
    bIsDead = true;
    MARK_PROPERTY_DIRTY_FROM_NAME(AHama, bIsDead, this);

    if (CurrentWeapon == ThirdWeapon)
    {
        CurrentWeapon = IsValid(PrimaryWeapon) ? PrimaryWeapon : SecondaryWeapon;

        if (CurrentWeapon)
        {
            CurrentWeapon->SetActorHiddenInGame(false);
            CurrentWeapon->EquipWeapon(this);
        }
        MARK_PROPERTY_DIRTY_FROM_NAME(AHama, CurrentWeapon, this);

        if (GetNetMode() != NM_DedicatedServer)
        {
            OnRep_CurrentWeapon();
        }
    }

    if (IsValid(ThirdWeapon))
    {
        ThirdWeapon->Destroy();
        ThirdWeapon = nullptr;
        MARK_PROPERTY_DIRTY_FROM_NAME(AHama, ThirdWeapon, this);
    }

    if (IsLocallyControlled())
    {
        OnPerksChangedEvent.ExecuteIfBound(OwnedPerks);
    }
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
        }

        if (IsSprinting())
        {
            HamaComponent->StopSprinting();
        }
    }

    UStaticMesh* BottleMesh = TargetPerk->GetBottleMesh();
    if (BottleMesh && PerkBottleMesh)
    {
        PerkBottleMesh->SetStaticMesh(BottleMesh);
        PerkBottleMesh->SetVisibility(true);
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
    if (PerkBottleMesh)
    {
        PerkBottleMesh->SetVisibility(false);
        PerkBottleMesh->SetStaticMesh(nullptr);
    }
}

void AHama::OnRep_OwnedPerks()
{
    if (IsLocallyControlled())
    {
        OnPerksChangedEvent.ExecuteIfBound(OwnedPerks);
    }

    if (OwnedPerks.Num() > 0)
    {
        FName LatestPerk = OwnedPerks.Last();
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("Client Received Perk: %s"), *LatestPerk.ToString()));
    }
}

float AHama::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (!HasAuthority() || bIsDead) return 0.f;

    float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    if (HealthComponent && AppliedDamage > 0.f)
    {
        HealthComponent->ApplyDamage(AppliedDamage, DamageCauser);
    }

    return AppliedDamage;
}

void AHama::Client_ShowDamageIndicator_Implementation(FVector DamageOrigin)
{
    if (!IsLocallyControlled()) return;

    FVector PlayerLocation = GetActorLocation();


    DamageOrigin.Z = PlayerLocation.Z;

    FVector DamageDirection = (DamageOrigin - PlayerLocation).GetSafeNormal();

    FVector Forward = GetActorForwardVector();
    FVector Right = GetActorRightVector();

    float ForwardDot = FVector::DotProduct(Forward, DamageDirection);
    float RightDot = FVector::DotProduct(Right, DamageDirection);
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
                if (CurrentWeapon && CurrentWeapon->IsReloading())
                {
                    CurrentWeapon->CancelReload();
                }

                bIsCurrentlyReviving = true;
                Server_BeginRevive(DownedPlayer);
            }
            else
            {

                if (FocusedInteractable->ShouldCancelReloadOnInteract())
                {
                    if (CurrentWeapon && CurrentWeapon->IsReloading())
                    {
                        CurrentWeapon->CancelReload();
                    }
                }

                if (FocusedInteractable->Client_PreInteract(this))
                {
                    Server_Interact(InteractActor);
                }
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
    if (!IsLocallyControlled() && !HasAuthority()) return;

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
    if (CurrentWeapon) Params.AddIgnoredActor(CurrentWeapon);

    FHitResult HitResult;
    FCollisionShape MeleeSphere = FCollisionShape::MakeSphere(MeleeRadius);

    bool bHit = GetWorld()->SweepSingleByChannel(
        HitResult, StartLocation, EndLocation, FQuat::Identity,
        ECC_Bullet,
        MeleeSphere, Params
    );

    if (bHit && HitResult.GetActor())
    {
        AZombie* HitZombie = Cast<AZombie>(HitResult.GetActor());
        if (HitZombie && !HitZombie->IsDead())
        {
            if (!HasAuthority())
            {
                Server_ValidateMeleeHit(HitZombie, HitResult.ImpactPoint);
            }
            else
            {
                FPointDamageEvent PointDamageEvent;
                PointDamageEvent.Damage = MeleeDamage;
                PointDamageEvent.HitInfo = HitResult;
                PointDamageEvent.ShotDirection = ViewRotation.Vector();
                PointDamageEvent.DamageTypeClass = UMeleeDamageType::StaticClass();

                HitZombie->TakeDamage(MeleeDamage, PointDamageEvent, OwnerController, this);
            }
        }
    }
}

bool AHama::Server_ValidateMeleeHit_Validate(AActor* HitActor, FVector_NetQuantize HitLocation)
{
    if (!HitActor || !HitActor->IsA(AZombie::StaticClass()))
    {
        return false;
    }

    return true;
}

void AHama::Server_ValidateMeleeHit_Implementation(AActor* HitActor, FVector_NetQuantize HitLocation)
{
    AZombie* HitZombie = Cast<AZombie>(HitActor);
    if (!HitZombie || HitZombie->IsDead()) return;

    // --- Anti-Cheat Check ---
    float Distance = FVector::Dist(GetActorLocation(), HitZombie->GetActorLocation());
    float MaxAllowedDistance = MeleeRange + MeleeRadius + 150.f;

    if (Distance <= MaxAllowedDistance)
    {
        FPointDamageEvent PointDamageEvent;
        PointDamageEvent.Damage = MeleeDamage;

        FHitResult ServerHit(HitZombie, nullptr, HitLocation, HitLocation);

        PointDamageEvent.HitInfo = ServerHit;
        PointDamageEvent.ShotDirection = (HitLocation - GetActorLocation()).GetSafeNormal();
        PointDamageEvent.DamageTypeClass = UMeleeDamageType::StaticClass();

        HitZombie->TakeDamage(MeleeDamage, PointDamageEvent, OwnerController, this);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Melee Exploit Detected! Player %s attacked from too far!"), *GetName());
    }
}

bool AHama::CanInteract(AHama* InteractingPlayer)
{
    if (!IsValid(InteractingPlayer) || InteractingPlayer == this)
    {
        return false;
    }

    if (InteractingPlayer->IsDowned() ||
        InteractingPlayer->bIsDead ||
        InteractingPlayer->IsDrinkingPerk())
    {
        return false;
    }

    if (!IsDowned() || bIsDead)
    {
        return false;
    }

    if (HealthComponent && HealthComponent->IsBeingRevived())
    {
       return false;
    }

    return true;
}

FString AHama::GetInteractMessage(AHama* InteractingPlayer)
{
    return FString(TEXT("Hold [F/X] To Revive"));
}

void AHama::Interact(AHama* InteractingPlayer)
{
}

bool AHama::Client_PreInteract(AHama* Player)
{
    return HamaComponent && !HamaComponent->IsDowned() && !bIsDead;
}

void AHama::Server_BeginRevive_Implementation(AHama* DownedPlayer)
{
    if (!DownedPlayer || !DownedPlayer->HealthComponent) return;
    if (DownedPlayer->HealthComponent->IsBeingRevived()) return;
    if (bIsDead || IsDowned() || !DownedPlayer->HealthComponent->IsDowned()) return;

    const float MaxAllowedDistSq = FMath::Square(SetIntractDistance + 100.f);
    if (FVector::DistSquared(GetActorLocation(), DownedPlayer->GetActorLocation()) > MaxAllowedDistSq)
    {
        return;
    }

    ClearAllReviveTimers();

    CurrentReviveTarget = DownedPlayer;
    DownedPlayer->HealthComponent->SetBeingRevived(true);

    float ReviveTime = DefaultReviveTime;

    ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>();

    if (bHasQuickRevive || (GS && GS->IsTeamAdrenalineActive()))
    {
        ReviveTime *= 0.5f;
    }
    if (HamaAbilityComponent && HamaAbilityComponent->GetAssignedAbility() == EHamaAbilityType::MedicalSupport)
    {
        ReviveTime *= 0.5f;
    }

    FTimerDelegate ReviveDel;
    ReviveDel.BindUObject(this, &AHama::Server_CompleteRevive);
    GetWorldTimerManager().SetTimer(ReviveTimerHandle, ReviveDel, ReviveTime, false);

    GetWorldTimerManager().SetTimer(
        ReviveCheckTimerHandle,
        this,
        &AHama::Server_CheckReviveConditions,
        0.1f,
        true
    );
}

void AHama::Server_CheckReviveConditions()
{
    if (bIsDead || IsDowned())
    {
        Server_CancelRevive();
        return;
    }

    AHama* Target = CurrentReviveTarget.Get();

    if (!IsValid(Target) || !Target->HealthComponent || !Target->HealthComponent->IsDowned())
    {
        Server_CancelRevive();
        return;
    }

    const float MaxAllowedDistSq = FMath::Square(SetIntractDistance + 100.f);
    if (FVector::DistSquared(GetActorLocation(), Target->GetActorLocation()) > MaxAllowedDistSq)
    {
        Server_CancelRevive();
        return;
    }
}

void AHama::Server_CancelRevive_Implementation()
{
    ClearAllReviveTimers();
}

void AHama::ClearAllReviveTimers()
{
    GetWorldTimerManager().ClearTimer(ReviveTimerHandle);
    GetWorldTimerManager().ClearTimer(ReviveCheckTimerHandle);

    if (AHama* Target = CurrentReviveTarget.Get())
    {
        if (Target->HealthComponent)
        {
            Target->HealthComponent->SetBeingRevived(false);
        }
    }

    CurrentReviveTarget = nullptr;
}

void AHama::Server_CompleteRevive()
{
    GetWorldTimerManager().ClearTimer(ReviveCheckTimerHandle);

    if (bIsDead || IsDowned())
    {
        ClearAllReviveTimers();
        return;
    }

    AHama* Target = CurrentReviveTarget.Get();

    if (IsValid(Target) && Target->HealthComponent)
    {
        if (Target->HealthComponent->IsDowned())
        {
            Target->HealthComponent->Revive();

            if (AHamaPlayerState* MyPS = GetPlayerState<AHamaPlayerState>())
            {
                MyPS->AddPoints(100);
            }
        }
    }

    ClearAllReviveTimers();
}

void AHama::Client_OnStaminUpAcquired_Implementation(float NewMaxStamina)
{
    if (HamaComponent)
    {
        HamaComponent->UpgradeMaxStamina(NewMaxStamina);
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Stamina Up Perk Acquired! Stamina Increased!"));
    }
}

void AHama::Client_OnPlayerDowned_Implementation()
{
    if (HamaComponent)
    {
        HamaComponent->ResetStamina();
    }
}