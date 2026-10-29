#include "Hama.h"
#include "HamaMovementComponent.h"
#include "HamaPlayerState.h"
#include "HamaAnimInstance.h"
#include "HamaPlayerController.h"
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

    bAlwaysRelevant = true;

    HamaComponent = CreateDefaultSubobject<UHamaComponent>(TEXT("HamaComponent"));
    HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
    HamaAbilityComponent = CreateDefaultSubobject<UHamaAbilityComponent>(TEXT("HamaAbilityComponent"));
    ThrowableComponent = CreateDefaultSubobject<UThrowableComponent>(TEXT("ThrowableComponent"));
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
    PerkBottleMesh->SetupAttachment(GetMesh(), FName("PerkBottleSocket"));

    GrenadeHandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GrenadeHandMesh"));
    GrenadeHandMesh->SetupAttachment(GetMesh(), TEXT("GrenadeHandSocket"));
    GrenadeHandMesh->SetVisibility(false);
    GrenadeHandMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    MonkeyHandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MonkeyHandMesh"));
    MonkeyHandMesh->SetupAttachment(GetMesh(), TEXT("GrenadeHandSocket"));
    MonkeyHandMesh->SetVisibility(false);
    MonkeyHandMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

const float AHama::CrossHairTimer = 0.05f;

// -----------------------------------------------------------------------------
// Gameplay Lifecycle
// -----------------------------------------------------------------------------
void AHama::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        if (HealthComponent)
        {
            HealthComponent->OnDownStateChanged.AddUObject(this, &AHama::HandleDownStateChanged);
        }

        if (UWorld* World = GetWorld())
        {
            if (UZombieDirectorSubsystem* Director = World->GetSubsystem<UZombieDirectorSubsystem>())
            {
                Director->RegisterPlayer(this);
            }
        }

        CreateDefaultWeapon();
        InitializeDownedWeapon();
    }
}

void AHama::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
    GetWorldTimerManager().ClearTimer(CrossHairTimerHandle);
    GetWorldTimerManager().ClearTimer(InteractTimerHandle);
    
    if (HasAuthority())
    {
        if (UWorld* World = GetWorld())
        {
            if (UZombieDirectorSubsystem* Director = World->GetSubsystem<UZombieDirectorSubsystem>())
            {
                Director->UnregisterPlayer(this);
            }
        }

        ClearAllReviveTimers();
    }
}

void AHama::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Params;
    Params.bIsPushBased = true;

    Params.Condition = COND_None;
    DOREPLIFETIME_WITH_PARAMS_FAST(AHama, CurrentWeapon, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AHama, bIsDead, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AHama, OwnedPerks, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AHama, bIsInFirstPerson, Params);

    Params.Condition = COND_OwnerOnly;
    DOREPLIFETIME_WITH_PARAMS_FAST(AHama, PrimaryWeapon, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AHama, SecondaryWeapon, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AHama, ThirdWeapon, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AHama, bIsDeathMachineActive, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AHama, CurrentlyUpgradingWeaponClass, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AHama, ReviveEndTime, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AHama, ReviveTotalDuration, Params);

    Params.Condition = COND_SkipOwner;
    DOREPLIFETIME_WITH_PARAMS_FAST(AHama, MeleeCounter, Params);
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

    OwnerController = Cast<AHamaPlayerController>(NewController);

    StartCrossHairTimer();
}

void AHama::UnPossessed()
{
    Super::UnPossessed();

    if (UHamaMovementComponent* MoveComp = HamaMovementComponent)
    {
        MoveComp->StopMovementImmediately();
        MoveComp->ClearAccumulatedForces();
    }

    ConsumeMovementInputVector();
}

void AHama::OnRep_Controller()
{
    Super::OnRep_Controller();

    OwnerController = Cast<AHamaPlayerController>(GetController());

    if(OwnerController && OwnerController->IsLocalController())
    {
        OwnerController->InitializeLocalPawn(this);
    }

    StartCrossHairTimer();
}

void AHama::PawnClientRestart()
{
    Super::PawnClientRestart();

    OwnerController = Cast<AHamaPlayerController>(GetController());

    if (IsLocallyControlled() && InteractSphere)
    {
        InteractSphere->OnComponentBeginOverlap.RemoveDynamic(this, &AHama::OnInteractSphereBeginOverlap);
        InteractSphere->OnComponentEndOverlap.RemoveDynamic(this, &AHama::OnInteractSphereEndOverlap);

        InteractSphere->OnComponentBeginOverlap.AddDynamic(this, &AHama::OnInteractSphereBeginOverlap);
        InteractSphere->OnComponentEndOverlap.AddDynamic(this, &AHama::OnInteractSphereEndOverlap);

        NearbyInteractables.Empty();

        TArray<AActor*> InitialOverlaps;
        InteractSphere->GetOverlappingActors(InitialOverlaps);

        for (AActor* Actor : InitialOverlaps)
        {
            if (Actor && Actor != this && Actor->Implements<UInteractInterface>())
            {
                NearbyInteractables.Add(Actor);
            }
        }

        if (!NearbyInteractables.IsEmpty())
        {
            GetWorldTimerManager().SetTimer(InteractTimerHandle, this, &AHama::CheckForInteractables, 0.1f, true);
        }
        else
        {
            GetWorldTimerManager().ClearTimer(InteractTimerHandle);
            ClearInteractFocus();
        }

        StartCrossHairTimer();
    }
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
        NearbyInteractables.Add(OtherActor);

        if (!GetWorldTimerManager().IsTimerActive(InteractTimerHandle))
        {
            GetWorldTimerManager().SetTimer(InteractTimerHandle, this, &AHama::CheckForInteractables, 0.1f, true);
        }
    }
}

void AHama::OnInteractSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (OtherActor && OtherActor != this && OtherActor->Implements<UInteractInterface>())
    {
        NearbyInteractables.Remove(OtherActor);
        NearbyInteractables.Remove(nullptr);

        if (FocusedInteractable.Get() == OtherActor)
        {
            ClearInteractFocus();
        }

        if (NearbyInteractables.IsEmpty())
        {
            GetWorldTimerManager().ClearTimer(InteractTimerHandle);
            ClearInteractFocus();
        }
    }
}

void AHama::CheckForInteractables()
{
    GEngine->AddOnScreenDebugMessage(-1, 0.1f, FColor::Green, FString::Printf(TEXT("Nearby Interactables: %d"), NearbyInteractables.Num()));
    if (!IsLocallyControlled() || !OwnerController) return;

    if (HamaComponent && HamaComponent->IsDowned())
    {
        ClearInteractFocus();
        return;
    }

    FVector CameraLocation;
    FRotator ViewRotation;
    OwnerController->GetPlayerViewPoint(CameraLocation, ViewRotation);


    FVector StartLocation = GetActorLocation() + FVector(0.f, 0.f, 50.f);
    FVector EndLocation = StartLocation + (ViewRotation.Vector() * SetIntractDistance);

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    Params.bReturnPhysicalMaterial = false;

    FCollisionShape Sphere = FCollisionShape::MakeSphere(45.f);

    DrawDebugLine(GetWorld(), StartLocation, EndLocation, FColor::Yellow, false, 0.1f, 0, 1.5f);

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
    if (NearbyInteractables.IsEmpty())
    {
        ClearInteractFocus();
        return;
    }

    if (Datum.OutHits.IsEmpty() || !Datum.OutHits[0].GetActor())
    {
        ClearInteractFocus();
        return;
    }

    AActor* HitActor = Datum.OutHits[0].GetActor();
    IInteractInterface* NewFocus = Cast<IInteractInterface>(HitActor);

    if (!NewFocus)
    {
        ClearInteractFocus();
        return;
    }

    if (HitActor != FocusedInteractable.Get())
    {
        FocusedInteractable = HitActor;
    }

    if (NewFocus->CanInteract(this))
    {
        OnInteractUpdateEvent.ExecuteIfBound(NewFocus->GetInteractMessage(this));
    }
    else
    {
        OnInteractUpdateEvent.ExecuteIfBound(TEXT(""));
    }
}

void AHama::ClearInteractFocus()
{
    if (FocusedInteractable)
    {
        FocusedInteractable = nullptr;
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
        UpdateAnimInstanceWeapon();
    }
}

TArray<TSubclassOf<ABaseWeapon>> AHama::GetOwnedWeaponClasses() const
{
    TArray<TSubclassOf<ABaseWeapon>> OwnedClasses;
    OwnedClasses.Reserve(4);

    if (IsValid(PrimaryWeapon))          OwnedClasses.Add(PrimaryWeapon->GetClass());
    if (IsValid(SecondaryWeapon))        OwnedClasses.Add(SecondaryWeapon->GetClass());
    if (IsValid(ThirdWeapon))            OwnedClasses.Add(ThirdWeapon->GetClass());
    if (CurrentlyUpgradingWeaponClass)   OwnedClasses.Add(CurrentlyUpgradingWeaponClass);

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
    else if (!ThirdWeapon && HasMuleKick())
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

void AHama::UpdateAnimInstanceWeapon()
{
    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        if (UHamaAnimInstance* HamaAnim = Cast<UHamaAnimInstance>(MeshComp->GetAnimInstance()))
        {
            HamaAnim->SetEquippedWeapon(CurrentWeapon);
        }
    }
}

void AHama::OnRep_CurrentWeapon(ABaseWeapon* PreviousWeapon)
{
    if (IsValid(PreviousWeapon) && PreviousWeapon != CurrentWeapon)
    {
        PreviousWeapon->OnAmmoChanged.Unbind();
        PreviousWeapon->OnWeaponDataInitialized.RemoveAll(this);
        PreviousWeapon->SetActorHiddenInGame(true);
    }

    if (!IsValid(CurrentWeapon))
    {
        UpdateAnimInstanceWeapon();
        if (IsLocallyControlled())
        {
            HandleAmmoChanged(0, 0);
        }
        return;
    }

    if (PendingWeaponForSwap == CurrentWeapon)
    {
        PendingWeaponForSwap = nullptr;
    }

    const bool bIsThrowing = (ThrowableComponent && ThrowableComponent->IsThrowingInProcess() || IsBrawlerModeActive());
    CurrentWeapon->SetActorHiddenInGame(bIsThrowing);
    AttachWeaponToMesh(CurrentWeapon);

    CurrentWeapon->OnWeaponDataInitialized.RemoveAll(this);

    if (!CurrentWeapon->IsDataInitialized())
    {
        CurrentWeapon->OnWeaponDataInitialized.AddUObject(this, &AHama::UpdateAnimInstanceWeapon);

        if (IsLocallyControlled())
        {
            CurrentWeapon->InitializeWeaponData();
        }
    }
    else
    {
        UpdateAnimInstanceWeapon();
    }

    if (IsLocallyControlled())
    {
        CurrentWeapon->OnAmmoChanged.Unbind();
        CurrentWeapon->OnAmmoChanged.BindUObject(this, &AHama::HandleAmmoChanged);
        HandleAmmoChanged(CurrentWeapon->GetCurrentAmmo(), CurrentWeapon->GetReserveAmmo());
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

    if (ThrowableComponent)
    {
        ThrowableComponent->RefillMonkeyToMax();
        ThrowableComponent->RefillGrenadesToMax();
    }
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

    if (CurrentWeapon)
    {
        CurrentWeapon->SetActorHiddenInGame(false);
        CurrentWeapon->EquipWeapon(this);
        AttachWeaponToMesh(CurrentWeapon);
    }

    OnRep_CurrentWeapon(WeaponToDestroy);

    WeaponToDestroy->Destroy();
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

bool AHama::IsSwappingWeapon() const
{
    if (PendingWeaponForSwap != nullptr)
    {
        return true;
    }

    if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
    {
        if (SwapWeaponMontage && AnimInstance->Montage_IsPlaying(SwapWeaponMontage))
        {
            return true;
        }
    }

    return false;
}

float AHama::GetSwapPlayRate() const
{
    float PlayRate = 1.0f;
    if (ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>())
    {
        if (HasFastHands())
        {
            PlayRate = 2.0f;
        }
    }
    return PlayRate;
}

void AHama::Input_SwapWeapon()
{
    SwapWeapon(nullptr);
}

void AHama::SwapWeapon(ABaseWeapon* TargetWeapon)
{
    if (IsBrawlerModeActive()) return;
    if (!SwapWeaponMontage || !CurrentWeapon || bIsDead) return;
    if (ThrowableComponent && ThrowableComponent->IsThrowingInProcess()) return;
    if (IsDrinkingPerk() || IsDowned()) return;

    UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
    if (!AnimInstance) return;

    ABaseWeapon* NextWeapon = TargetWeapon;

    // بۆ YY: ئەگەر لە ناوەڕاستی Swap دابین، لەسەر بنەمای چەکە چاوەڕوانکراوەکە چەکی دواتر دەدۆزینەوە
    ABaseWeapon* ReferenceWeapon = IsValid(PendingWeaponForSwap) ? PendingWeaponForSwap : CurrentWeapon;

    if (!NextWeapon)
    {
        if (bIsDeathMachineActive)
        {
            NextWeapon = IsValid(PreDeathMachineWeapon) ? PreDeathMachineWeapon : PrimaryWeapon;
        }
        else if (ReferenceWeapon == PrimaryWeapon)
        {
            NextWeapon = IsValid(SecondaryWeapon) ? SecondaryWeapon : ThirdWeapon;
        }
        else if (ReferenceWeapon == SecondaryWeapon)
        {
            NextWeapon = IsValid(ThirdWeapon) ? ThirdWeapon : PrimaryWeapon;
        }
        else if (ReferenceWeapon == ThirdWeapon)
        {
            NextWeapon = IsValid(PrimaryWeapon) ? PrimaryWeapon : SecondaryWeapon;
        }
    }

    if (!IsValid(NextWeapon)) return;

   
    bool bIsYYCancel = (PendingWeaponForSwap != nullptr && NextWeapon == CurrentWeapon);

    if (!bIsYYCancel)
    {
        if (GetWorld()->GetTimeSeconds() - PendingSwapStartTime < 0.04f) return;
    }

    PendingSwapStartTime = GetWorld()->GetTimeSeconds();

    if (PendingWeaponForSwap != nullptr && NextWeapon == CurrentWeapon)
    {
        FOnMontageEnded EmptyDelegate;
        AnimInstance->Montage_SetEndDelegate(EmptyDelegate, SwapWeaponMontage);

        if (IsValid(PendingWeaponForSwap) && PendingWeaponForSwap != CurrentWeapon)
        {
            PendingWeaponForSwap->SetActorHiddenInGame(true);
        }

        PendingWeaponForSwap = nullptr;

        CurrentWeapon->SetActorHiddenInGame(false);
        AttachWeaponToMesh(CurrentWeapon);

        AnimInstance->Montage_Stop(0.15f, SwapWeaponMontage);

        Server_SwapWeapon(CurrentWeapon);
        return;
    }

    if (NextWeapon == ReferenceWeapon) return;

    float TargetPlayRate = GetSwapPlayRate();

    FOnMontageEnded EmptyDelegate;
    AnimInstance->Montage_SetEndDelegate(EmptyDelegate, SwapWeaponMontage);

    if (AnimInstance->Montage_Play(SwapWeaponMontage, TargetPlayRate) <= 0.0f)
    {
        return;
    }

    if (bIsFireButtonHold) CurrentWeapon->StopFire();
    if (CurrentWeapon->IsReloading()) CurrentWeapon->CancelReload();
    if (IsSprinting()) StopSprint();

    if (IsValid(PendingWeaponForSwap) && PendingWeaponForSwap != CurrentWeapon)
    {
        PendingWeaponForSwap->SetActorHiddenInGame(true);
    }

    PendingWeaponForSwap = NextWeapon;

    Server_SwapWeapon(NextWeapon);

    FOnMontageEnded MontageEndedDelegate;
    MontageEndedDelegate.BindUObject(this, &AHama::OnSwapWeaponMontageEnded);
    AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, SwapWeaponMontage);
}

void AHama::Server_SwapWeapon_Implementation(ABaseWeapon* NewWeapon)
{
    if (IsBrawlerModeActive()) return;
    if (!SwapWeaponMontage || !CurrentWeapon || bIsDead) return;
    if (IsDrinkingPerk() || IsDowned()) return;
    if (ThrowableComponent && ThrowableComponent->IsThrowingInProcess()) return;
    if (!IsValid(NewWeapon)) return;

    if (PendingWeaponForSwap != nullptr && NewWeapon == CurrentWeapon)
    {
        PendingWeaponForSwap = nullptr;

        if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
        {
            AnimInstance->Montage_Stop(0.15f, SwapWeaponMontage);
        }

        Multicast_StopSwapMontage();
        return;
    }

    if (NewWeapon == CurrentWeapon || NewWeapon == PendingWeaponForSwap)
    {
        return;
    }

    if (bIsDeathMachineActive)
    {
        if (NewWeapon != PreDeathMachineWeapon && NewWeapon != PrimaryWeapon && NewWeapon != SecondaryWeapon && NewWeapon != ThirdWeapon)
        {
            return;
        }
    }
    else
    {
        if (NewWeapon != PrimaryWeapon && NewWeapon != SecondaryWeapon && NewWeapon != ThirdWeapon)
        {
            return;
        }
    }

    PendingWeaponForSwap = NewWeapon;
    PendingSwapStartTime = GetWorld()->GetTimeSeconds();

    Multicast_PlaySwapMontage(GetSwapPlayRate());
}

void AHama::HandleWeaponSwapNotify()
{
    if (IsLocallyControlled())
    {
        if (IsValid(PendingWeaponForSwap))
        {
            if (IsValid(CurrentWeapon))
            {
                CurrentWeapon->SetActorHiddenInGame(true);
            }

            const bool bIsThrowing = ThrowableComponent && ThrowableComponent->IsThrowingInProcess();
            PendingWeaponForSwap->SetActorHiddenInGame(bIsThrowing);
            AttachWeaponToMesh(PendingWeaponForSwap);
        }

        Server_CompleteWeaponSwap();
    }
}

void AHama::Server_CompleteWeaponSwap_Implementation()
{
    if (!IsValid(PendingWeaponForSwap)) return;

    CompleteWeaponSwap();
}

void AHama::CompleteWeaponSwap()
{
    if (!HasAuthority() || !PendingWeaponForSwap) return;

    ABaseWeapon* OldWeapon = CurrentWeapon;
    const bool bWasDeathMachine = (OldWeapon && OldWeapon == ActiveDeathMachine);

    if (IsValid(OldWeapon))
    {
        OldWeapon->StopFire();
        if (OldWeapon->IsReloading()) OldWeapon->CancelReload();
    }

    if (bWasDeathMachine)
    {
        bIsDeathMachineActive = false;
        MARK_PROPERTY_DIRTY_FROM_NAME(AHama, bIsDeathMachineActive, this);

        GetWorldTimerManager().ClearTimer(DeathMachineTimerHandle);
        GetWorldTimerManager().ClearTimer(SwapFailSafeHandle);

        ActiveDeathMachine = nullptr;

        PreDeathMachineWeapon = nullptr;
        MARK_PROPERTY_DIRTY_FROM_NAME(AHama, PreDeathMachineWeapon, this);
    }
    else if (IsValid(OldWeapon))
    {
        OldWeapon->SetActorHiddenInGame(true);
        OldWeapon->SetActorEnableCollision(false);
    }

    // ١. سەرەتا CurrentWeapon دەگۆڕین بۆ چەکە نوێیەکە
    CurrentWeapon = PendingWeaponForSwap;
    MARK_PROPERTY_DIRTY_FROM_NAME(AHama, CurrentWeapon, this);

    CurrentWeapon->SetActorHiddenInGame(false);
    CurrentWeapon->EquipWeapon(this);

    ABaseWeapon* NewlyEquippedWeapon = PendingWeaponForSwap;
    PendingWeaponForSwap = nullptr;

    // ٢. ئێستا کە CurrentWeapon بووە بە چەکە نوێیەکە، بە دەست OnRep بانگ دەکەین بۆ Host!
    OnRep_CurrentWeapon(OldWeapon);

    // ٣. ئەگەر چەکە کۆنەکە DeathMachine بوو، دوای Unbind بوونی لە ناو OnRep دەیسڕینەوە
    if (bWasDeathMachine && IsValid(OldWeapon))
    {
        OldWeapon->Destroy();
    }

    if (IsValid(NewlyEquippedWeapon) && NewlyEquippedWeapon->NeedsAmmo() && NewlyEquippedWeapon->CanReload())
    {
        NewlyEquippedWeapon->Reload();
    }
}

void AHama::OnSwapWeaponMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
    if (AnimInstance && AnimInstance->Montage_IsPlaying(SwapWeaponMontage))
    {
        return;
    }

    PendingWeaponForSwap = nullptr;
}


void AHama::GiveDeathMachine(TSubclassOf<ABaseWeapon> WeaponClass, float Duration)
{
    if (!HasAuthority() || !WeaponClass || bIsDead) return;
    if (HamaComponent && HamaComponent->IsDowned()) return;

    PendingWeaponForSwap = nullptr;
    GetWorldTimerManager().ClearTimer(SwapFailSafeHandle);

    if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
    {
        if (SwapWeaponMontage && AnimInstance->Montage_IsPlaying(SwapWeaponMontage))
        {
            FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(SwapWeaponMontage);
            if (MontageInstance) MontageInstance->OnMontageEnded.Unbind();
            AnimInstance->Montage_Stop(0.1f, SwapWeaponMontage);
        }
    }

    Multicast_StopSwapMontage();

    if (bIsDeathMachineActive && IsValid(ActiveDeathMachine))
    {
        GetWorldTimerManager().SetTimer(DeathMachineTimerHandle, this, &AHama::RemoveDeathMachine, Duration, false);
        return;
    }

    ABaseWeapon* OldWeapon = CurrentWeapon;

    if (IsValid(OldWeapon) && OldWeapon != ActiveDeathMachine)
    {
        OldWeapon->StopFire();
        if (OldWeapon->IsReloading()) OldWeapon->CancelReload();
        OldWeapon->SetActorHiddenInGame(true);
        OldWeapon->SetActorEnableCollision(false);

        PreDeathMachineWeapon = OldWeapon;
        MARK_PROPERTY_DIRTY_FROM_NAME(AHama, PreDeathMachineWeapon, this);
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

        OnRep_CurrentWeapon(OldWeapon);
    }

    GetWorldTimerManager().SetTimer(DeathMachineTimerHandle, this, &AHama::RemoveDeathMachine, Duration, false);
}

void AHama::RemoveDeathMachine()
{
    if (!HasAuthority()) return;

    if (HamaComponent && (HamaComponent->IsDowned() || bIsDead))
    {
        if (IsValid(ActiveDeathMachine))
        {
            ActiveDeathMachine->StopFire();
            ActiveDeathMachine->Destroy();
            ActiveDeathMachine = nullptr;
        }
        bIsDeathMachineActive = false;
        MARK_PROPERTY_DIRTY_FROM_NAME(AHama, bIsDeathMachineActive, this);
        return;
    }

    ABaseWeapon* WeaponToEquip = IsValid(PreDeathMachineWeapon) ? PreDeathMachineWeapon : PrimaryWeapon;

    if (ThrowableComponent && ThrowableComponent->IsThrowingInProcess())
    {
        ABaseWeapon* OldDeathMachine = ActiveDeathMachine;
        ActiveDeathMachine = nullptr;

        bIsDeathMachineActive = false;
        MARK_PROPERTY_DIRTY_FROM_NAME(AHama, bIsDeathMachineActive, this);

        PreDeathMachineWeapon = nullptr;
        MARK_PROPERTY_DIRTY_FROM_NAME(AHama, PreDeathMachineWeapon, this);

        if (IsValid(WeaponToEquip))
        {
            CurrentWeapon = WeaponToEquip;
            CurrentWeapon->EquipWeapon(this);
            MARK_PROPERTY_DIRTY_FROM_NAME(AHama, CurrentWeapon, this);

            if (GetNetMode() != NM_DedicatedServer)
            {
                OnRep_CurrentWeapon(OldDeathMachine);
            }
        }

        if (IsValid(OldDeathMachine))
        {
            OldDeathMachine->StopFire();
            OldDeathMachine->Destroy();
        }
        return;
    }

    if (!IsValid(WeaponToEquip)) return;

    Client_EndDeathMachineAndSwap(WeaponToEquip);

    float AnimDuration = (SwapWeaponMontage ? SwapWeaponMontage->GetPlayLength() : 1.0f) / GetSwapPlayRate();
    GetWorldTimerManager().SetTimer(
        SwapFailSafeHandle,
        FTimerDelegate::CreateWeakLambda(this, [this, WeaponToEquip]()
            {
                if (IsValid(ActiveDeathMachine) && IsValid(WeaponToEquip))
                {
                    PendingWeaponForSwap = WeaponToEquip;
                    CompleteWeaponSwap();
                }
            }),
        AnimDuration + 0.5f,
        false
    );
}

void AHama::Client_EndDeathMachineAndSwap_Implementation(ABaseWeapon* WeaponToEquip)
{
    if (!IsValid(WeaponToEquip)) return;

    SwapWeapon(WeaponToEquip);
}

void AHama::Server_CancelSwap_Implementation()
{
    PendingWeaponForSwap = nullptr;
}

void AHama::Multicast_PlaySwapMontage_Implementation(float PlayRate)
{
    if (IsLocallyControlled()) return;
    if (SwapWeaponMontage) PlayAnimMontage(SwapWeaponMontage, PlayRate);
}

void AHama::Multicast_StopSwapMontage_Implementation()
{
    if (IsLocallyControlled()) return;
    if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
    {
        if (SwapWeaponMontage && AnimInstance->Montage_IsPlaying(SwapWeaponMontage))
        {
            AnimInstance->Montage_Stop(0.1f, SwapWeaponMontage);
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

void AHama::InitializeDownedWeapon()
{
    if (!HasAuthority() || !DownedPistolClass || IsValid(DownedWeaponInstance)) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    DownedWeaponInstance = GetWorld()->SpawnActor<ABaseWeapon>(DownedPistolClass, GetActorTransform(), SpawnParams);
    if (DownedWeaponInstance)
    {
        DownedWeaponInstance->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
        DownedWeaponInstance->SetActorHiddenInGame(true);
        DownedWeaponInstance->SetActorEnableCollision(false);
        DownedWeaponInstance->SetActorTickEnabled(false);
    }
}

void AHama::HandleDownStateChanged(bool bIsDowned)
{
    if (!HasAuthority()) return;

    if (bIsDowned)
    {
        EquipDownedWeapon();
    }
    else
    {
        RestorePreDownWeapon();
    }
}

void AHama::EquipDownedWeapon()
{
    if (!HasAuthority()) return;

    if (HamaAbilityComponent) HamaAbilityComponent->StopAllAbilities();

    PendingWeaponForSwap = nullptr;
    GetWorldTimerManager().ClearTimer(SwapFailSafeHandle);
    Multicast_StopSwapMontage();

    ABaseWeapon* WeaponBeforeDown = CurrentWeapon;

    if (IsValid(WeaponBeforeDown))
    {
        WeaponBeforeDown->StopFire();
        if (WeaponBeforeDown->IsReloading()) WeaponBeforeDown->CancelReload();
    }

    if (bIsDeathMachineActive || (IsValid(ActiveDeathMachine) && WeaponBeforeDown == ActiveDeathMachine))
    {
        PreDownWeapon = IsValid(PreDeathMachineWeapon) ? PreDeathMachineWeapon : PrimaryWeapon;

        GetWorldTimerManager().ClearTimer(DeathMachineTimerHandle);

        if (IsValid(ActiveDeathMachine))
        {
            ActiveDeathMachine->StopFire();
            ActiveDeathMachine->Destroy();
            ActiveDeathMachine = nullptr;
        }

        bIsDeathMachineActive = false;
        MARK_PROPERTY_DIRTY_FROM_NAME(AHama, bIsDeathMachineActive, this);

        PreDeathMachineWeapon = nullptr;
        MARK_PROPERTY_DIRTY_FROM_NAME(AHama, PreDeathMachineWeapon, this);
    }
    else
    {
        PreDownWeapon = WeaponBeforeDown;
    }

    static const FGameplayTag DownedTag = FGameplayTag::RequestGameplayTag(FName("Weapon.Ability.CanBeUsedWhileDowned"));

    ABaseWeapon* TargetDownedWeapon = nullptr;

    TArray<ABaseWeapon*, TInlineAllocator<3>> InventoryWeapons = { PrimaryWeapon, SecondaryWeapon, ThirdWeapon };
    for (ABaseWeapon* Weapon : InventoryWeapons)
    {
        if (IsValid(Weapon) && Weapon->GetWeaponTags().HasTag(DownedTag))
        {
            TargetDownedWeapon = Weapon;
            break;
        }
    }

    const bool bUsingFallbackInstance = (TargetDownedWeapon == nullptr);
    if (bUsingFallbackInstance && IsValid(DownedWeaponInstance))
    {
        TargetDownedWeapon = DownedWeaponInstance;
    }

    if (TargetDownedWeapon)
    {
        CurrentWeapon = TargetDownedWeapon;
        CurrentWeapon->EquipWeapon(this);

        TargetDownedWeapon->ResetDownedAmmo(bUsingFallbackInstance);
    }

    MARK_PROPERTY_DIRTY_FROM_NAME(AHama, CurrentWeapon, this);
    OnRep_CurrentWeapon(WeaponBeforeDown);
}

void AHama::RestorePreDownWeapon()
{
    if (!HasAuthority()) return;

    ABaseWeapon* WeaponWhileDowned = CurrentWeapon;
    if (IsValid(WeaponWhileDowned))
    {
        WeaponWhileDowned->StopFire();
        if (WeaponWhileDowned->IsReloading()) WeaponWhileDowned->CancelReload();
    }

    if (IsValid(PreDownWeapon))
    {
        CurrentWeapon = PreDownWeapon;
    }
    else if (IsValid(PrimaryWeapon))
    {
        CurrentWeapon = PrimaryWeapon;
    }
    else if (IsValid(SecondaryWeapon))
    {
        CurrentWeapon = SecondaryWeapon;
    }
    else
    {
        CurrentWeapon = nullptr;
    }

    PreDownWeapon = nullptr;

    if (IsValid(CurrentWeapon))
    {
        CurrentWeapon->EquipWeapon(this);
    }

    MARK_PROPERTY_DIRTY_FROM_NAME(AHama, CurrentWeapon, this);
    OnRep_CurrentWeapon(WeaponWhileDowned);
}

void AHama::Destroyed()
{
    Super::Destroyed();

    if (HasAuthority())
    {
        if (DownedWeaponInstance)
        {
            DownedWeaponInstance->Destroy();
            DownedWeaponInstance = nullptr;
        }

        if (ActiveDeathMachine)
        {
            ActiveDeathMachine->Destroy();
            ActiveDeathMachine = nullptr;
        }

        if (PrimaryWeapon)
        {
            PrimaryWeapon->Destroy();
            PrimaryWeapon = nullptr;
        }

        if (SecondaryWeapon)
        {
            SecondaryWeapon->Destroy();
            SecondaryWeapon = nullptr;
        }

        if (ThirdWeapon)
        {
            ThirdWeapon->Destroy();
            ThirdWeapon = nullptr;
        }

        if (CurrentWeapon && IsValid(CurrentWeapon))
        {
            CurrentWeapon->Destroy();
            CurrentWeapon = nullptr;
        }
    }
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

        EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Triggered, this, &AHama::CrouchActionPressed);
        EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Completed, this, &AHama::CrouchActionReleased);
        EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Canceled, this, &AHama::CrouchActionReleased);

        EnhancedInput->BindAction(AimAction, ETriggerEvent::Started, this, &AHama::AimActionPressed);
        EnhancedInput->BindAction(AimAction, ETriggerEvent::Completed, this, &AHama::AimActionReleased);
        EnhancedInput->BindAction(AimAction, ETriggerEvent::Canceled, this, &AHama::AimActionReleased);

        EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &AHama::FireActionPressed);
        EnhancedInput->BindAction(FireAction, ETriggerEvent::Completed, this, &AHama::FireActionReleased);
        EnhancedInput->BindAction(FireAction, ETriggerEvent::Canceled, this, &AHama::FireActionReleased);

        EnhancedInput->BindAction(ReloadAction, ETriggerEvent::Started, this, &AHama::ReloadActionPressed);
        EnhancedInput->BindAction(AbilityAction, ETriggerEvent::Started, this, &AHama::AbilityActionPressed);
        EnhancedInput->BindAction(SwapWeaponAction, ETriggerEvent::Started, this, &AHama::Input_SwapWeapon);

        EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &AHama::InteractActionPressed);
        EnhancedInput->BindAction(InteractAction, ETriggerEvent::Completed, this, &AHama::InteractActionReleased);
        EnhancedInput->BindAction(InteractAction, ETriggerEvent::Canceled, this, &AHama::InteractActionReleased);

        EnhancedInput->BindAction(GamepadXAction, ETriggerEvent::Triggered, this, &AHama::GamepadXActionPressed);
        EnhancedInput->BindAction(GamepadXAction, ETriggerEvent::Completed, this, &AHama::GamepadXActionReleased);
        EnhancedInput->BindAction(GamepadXAction, ETriggerEvent::Canceled, this, &AHama::GamepadXActionReleased);

        EnhancedInput->BindAction(MeleeAction, ETriggerEvent::Started, this, &AHama::MeleeActionPressed);

        EnhancedInput->BindAction(IA_ThrowGrenade, ETriggerEvent::Started, this, &AHama::Input_ThrowGrenadePressed);
        EnhancedInput->BindAction(IA_ThrowGrenade, ETriggerEvent::Completed, this, &AHama::Input_ThrowGrenadeReleased);
        EnhancedInput->BindAction(IA_ThrowGrenade, ETriggerEvent::Canceled, this, &AHama::Input_ThrowGrenadeReleased);

        EnhancedInput->BindAction(IA_ThrowMonkey, ETriggerEvent::Started, this, &AHama::Input_ThrowMonkeyPressed);
        EnhancedInput->BindAction(IA_ThrowMonkey, ETriggerEvent::Completed, this, &AHama::Input_ThrowMonkeyReleased);
        EnhancedInput->BindAction(IA_ThrowMonkey, ETriggerEvent::Canceled, this, &AHama::Input_ThrowMonkeyReleased);
    }
}

// -----------------------------------------------------------------------------
// Input Callback Functions
// -----------------------------------------------------------------------------
void AHama::FireActionPressed()
{
    if (!HamaComponent) return;
    if (ThrowableComponent && ThrowableComponent->IsThrowingInProcess()) return;
    if (IsDrinkingPerk() || IsDowned()) return;

    if (IsBrawlerModeActive())
    {
        MeleeActionPressed();
        return;
    }

    if (!CurrentWeapon || IsMeleeing()) return;
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
   
    if (IsDrinkingPerk()) return;
    if (IsMeleeing()) return;

    if (IsBrawlerModeActive())
    {
        MeleeActionPressed();
        return;
    }

    if (!HamaComponent || !CurrentWeapon) return;

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

    FName TargetSocket = HasDeadshot() ? FName("head") : FName("spine_04");

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
    //if (!OwnerController) return;
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
    //if (!OwnerController) return;

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
        if (!bHasPerformedDive && Instance.GetElapsedTime() >= 0.25f)
        {
            bHasPerformedDive = true;
            StartDiving();
        }
    }
}

void AHama::CrouchActionReleased(const FInputActionInstance& Instance)
{
    bIsCrouchButtonHold = false;

    if (bHasPerformedDive)
    {
        bHasPerformedDive = false;
        return;
    }

    if (GetCharacterMovement()->IsFalling()) return;

    if (!HamaComponent || HamaComponent->IsSlide() || HamaComponent->IsDowned() || HamaComponent->IsDiving())
    {
        return;
    }

    if (Instance.GetElapsedTime() < 0.25f)
    {
        if (IsSprinting())
        {
            StartSlideRoutine();
        }
        else
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
    if (!HamaComponent->CanDive()) return;
    if (IsSprinting())  HamaComponent->StopSprinting();

    HamaComponent->StartDive();

    PlayAnimMontage(DiveMontage);
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

bool AHama::ShouldShowFirstPersonView() const
{
    if (IsLocallyControlled())
    {
        return bIsInFirstPerson;
    }

    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            if (PC->IsLocalController() && PC->GetViewTarget() == this)
            {
                return bIsInFirstPerson;
            }
        }
    }

    return false;
}

void AHama::UpdateCameraAndMeshVisibility()
{
    const bool bUseFPP = ShouldShowFirstPersonView();

    if (TPCamera && FPCamera)
    {
        FPCamera->SetActive(bUseFPP);
        TPCamera->SetActive(!bUseFPP);
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

            UpdateCameraAndMeshVisibility();

            if (!HasAuthority())
            {
                Server_SetFirstPersonState(bIsInFirstPerson);
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

void AHama::Server_SetFirstPersonState_Implementation(bool bNewState)
{
    bIsInFirstPerson = bNewState;
    MARK_PROPERTY_DIRTY_FROM_NAME(AHama, bIsInFirstPerson, this);

    UpdateCameraAndMeshVisibility();
}

void AHama::OnRep_IsInFirstPerson()
{
    UpdateCameraAndMeshVisibility();
}

bool AHama::IsMovingForward() const
{
    const FVector InputVector = GetLastMovementInputVector();
    if (InputVector.IsNearlyZero()) return false;

    const FVector Forward2D = GetActorForwardVector().GetSafeNormal2D();
    const FVector Input2D = InputVector.GetSafeNormal2D();

    return FVector::DotProduct(Forward2D, Input2D) > 0.5f;
}

void AHama::SprintActionPressed()
{
    if (!IsMovingForward()) return;
    if (!HamaComponent) return;
    if (!SetCanInteract()) return;
    if (IsSliding()) return;
    if (IsDiving()) return;
    if (HamaComponent->GetStamina() <= 20.f) return;
    if (GetCharacterMovement()->IsFalling()) return;
    if (HamaComponent->IsAiming())   OnAim(false);
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

void AHama::OnSprintStopped()
{
    if (!CurrentWeapon) return;

    if (bIsAimButtonHold)
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
    if (HealthComponent && HealthComponent->IsDowned()) return;
    if (IsDrinkingPerk()) return;
    if (!HamaAbilityComponent || !HamaAbilityComponent->IsPowerFull() || HamaAbilityComponent->IsAbilityActive()) return;

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
    if (!TargetPerk || bIsDead || !HasAuthority() || IsDowned()) return;
    if (DrinkingPerkTimer()) return;

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

    if (PerkID == FName(TEXT("Juggernaut")))
    {
        if (HealthComponent)
        {
            HealthComponent->UpgradeHealth(150.f);
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
}

void AHama::OnPlayerDowned()
{
    if (!HasAuthority()) return;

    if (ThrowableComponent)
    {
        ThrowableComponent->HandleOwnerDowned();
    }

    if (UWorld* World = GetWorld())
    {
        if (UZombieDirectorSubsystem* Director = World->GetSubsystem<UZombieDirectorSubsystem>())
        {
            Director->SetPlayerTargetable(this, false);
        }
    }

    if(CurrentWeapon)
    {
        CurrentWeapon->StopFire();
        if (CurrentWeapon->IsReloading())
        {
            CurrentWeapon->CancelReload();
        }
    }

    GetWorldTimerManager().ClearTimer(PerkDrinkTimerHandle);
    PendingPerkID = NAME_None;

    if (PerkBottleMesh)
    {
        PerkBottleMesh->SetVisibility(false);
        PerkBottleMesh->SetStaticMesh(nullptr);
    }

    if (HamaComponent)
    {
        HamaComponent->ResetStamina();
    }

    OwnedPerks.Empty();
    MARK_PROPERTY_DIRTY_FROM_NAME(AHama, OwnedPerks, this);

    if (IsLocallyControlled())
    {
        OnPerksChangedEvent.ExecuteIfBound(OwnedPerks);
    }

    // Bo Srenawai Chaki seyam Agar Pewest Bu
    /*if (IsValid(ThirdWeapon))
    {
        if (CurrentWeapon == ThirdWeapon)
        {
            CurrentWeapon = PrimaryWeapon ? PrimaryWeapon : SecondaryWeapon;
        }
        if (PreDeathMachineWeapon == ThirdWeapon)
        {
            PreDeathMachineWeapon = PrimaryWeapon ? PrimaryWeapon : SecondaryWeapon;
            MARK_PROPERTY_DIRTY_FROM_NAME(AHama, PreDeathMachineWeapon, this);
        }

        ThirdWeapon->StopFire();
        ThirdWeapon->Destroy();
        ThirdWeapon = nullptr;
        MARK_PROPERTY_DIRTY_FROM_NAME(AHama, ThirdWeapon, this);
    }*/

    ForceNetUpdate();
}

void AHama::HandleDownedStateClient(bool bDowned)
{
    if (bDowned)
    {
        if (IsSprinting())
        {
            StopSprint();
        }

        if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
        {
            AnimInstance->Montage_Stop(0.1f);
        }

        if (CurrentWeapon)
        {
            FireActionReleased();
            if (CurrentWeapon->IsReloading())
            {
                CurrentWeapon->CancelReload();
            }
        }
    }
}

void AHama::OnPlayerRevived()
{
    if (!HasAuthority()) return;

    if (UWorld* World = GetWorld())
    {
        if (UZombieDirectorSubsystem* Director = World->GetSubsystem<UZombieDirectorSubsystem>())
        {
            Director->SetPlayerTargetable(this, true);
        }
    }

    OwnedPerks.Empty();
    MARK_PROPERTY_DIRTY_FROM_NAME(AHama, OwnedPerks, this);

    if (IsLocallyControlled())
    {
        OnPerksChangedEvent.ExecuteIfBound(OwnedPerks);
    }

    ForceNetUpdate();
}

void AHama::HandleDeath()
{
    if (!HasAuthority()) return;

    GetWorldTimerManager().ClearTimer(PerkDrinkTimerHandle);
    PendingPerkID = NAME_None;

    if (PerkBottleMesh)
    {
        PerkBottleMesh->SetVisibility(false);
        PerkBottleMesh->SetStaticMesh(nullptr);
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
}

void AHama::Multicast_PlayDrinkPerkAnimation_Implementation(ABasePerk* TargetPerk)
{
    if (!TargetPerk || !DrinkPerkMontage) return;

    if (CurrentWeapon)
    {
        CurrentWeapon->StopFire();
        if (CurrentWeapon->IsReloading()) CurrentWeapon->CancelReload();
        CurrentWeapon->SetActorHiddenInGame(true);
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

    if (CurrentWeapon)
    {
        CurrentWeapon->SetActorHiddenInGame(false);
    }

    if (bInterrupted && HasAuthority())
    {
        GetWorldTimerManager().ClearTimer(PerkDrinkTimerHandle);
        PendingPerkID = NAME_None;
    }
}

void AHama::OnRep_OwnedPerks()
{
    if (IsLocallyControlled())
    {
        OnPerksChangedEvent.ExecuteIfBound(OwnedPerks);
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

    AActor* InteractActor = FocusedInteractable.Get();
    IInteractInterface* FocusedInterface = Cast<IInteractInterface>(InteractActor);
    if (FocusedInterface && FocusedInterface->CanInteract(this))
    {
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
                if (FocusedInterface->Client_PreInteract(this))
                {
                    if (FocusedInterface->ShouldCancelReloadOnInteract())
                    {
                        if (CurrentWeapon && CurrentWeapon->IsReloading())
                        {
                            CurrentWeapon->CancelReload();
                        }
                    }

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
        IInteractInterface* FocusedInterface = Cast<IInteractInterface>(FocusedInteractable.Get());
        if (FocusedInterface && FocusedInterface->CanInteract(this))
        {
            bIsxButtonHolded = true;
            InteractActionPressed();
        }
    }
}

void AHama::GamepadXActionReleased()
{
    if (bIsxButtonHolded)
    {
        bIsxButtonHolded = false;
        InteractActionReleased();
    }
    else
    {
        ReloadActionPressed();
    }
}

void AHama::Server_Interact_Implementation(AActor* InteractTarget)
{
    if (!InteractTarget) return;
    if (!SetCanInteract()) return;

    GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, FString::Printf(TEXT("Server_Interact called for: %s"), *InteractTarget->GetName()));
    const float DistSq = FVector::DistSquared(GetActorLocation(), InteractTarget->GetActorLocation());
    if (DistSq > FMath::Square(300.f)) return;

    IInteractInterface* Interface = Cast<IInteractInterface>(InteractTarget);
    if (Interface)
    {
        Interface->Interact(this);
    }
}

// -----------------------------------------------------------------------------
// Melee System Implementation
// -----------------------------------------------------------------------------

bool AHama::IsBrawlerModeActive() const
{
    return HamaAbilityComponent && HamaAbilityComponent->IsBrawlerActive();
}

UAnimMontage* AHama::GetActiveMeleeMontage() const
{
    if (IsBrawlerModeActive() && BrawlerAttackMontage)
    {
        return BrawlerAttackMontage;
    }
    return MeleeAttackMontage;
}

void AHama::PlayMeleeMontageByCounter(UAnimMontage* MontageToPlay)
{
    if (!MontageToPlay) return;

    const float Duration = PlayAnimMontage(MontageToPlay);
    if (Duration > 0.0f && MontageToPlay == BrawlerAttackMontage && BrawlerComboSections.Num() > 0)
    {
        if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
        {
            const int32 SectionIndex = MeleeCounter % BrawlerComboSections.Num();
            const FName SectionName = BrawlerComboSections[SectionIndex];
            if (MontageToPlay->IsValidSectionName(SectionName))
            {
                AnimInstance->Montage_JumpToSection(SectionName, MontageToPlay);
            }
        }
    }
}

void AHama::OnBrawlerStateChanged(bool bInBrawlerMode)
{
    UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;

    if (bInBrawlerMode)
    {
        // ١. هەڵوەشاندنەوەی هەر چەک گۆڕینێک کە لە نیوەدا بێت
        PendingWeaponForSwap = nullptr;
        GetWorldTimerManager().ClearTimer(SwapFailSafeHandle);
        if (AnimInstance && SwapWeaponMontage && AnimInstance->Montage_IsPlaying(SwapWeaponMontage))
        {
            AnimInstance->Montage_Stop(0.1f, SwapWeaponMontage);
        }

        // ٢. دەرچوون لە دۆخی نیشانەگرتن (Aim)
        if (bIsAimButtonHold && HamaComponent)
        {
            bIsAimButtonHold = false;
            HamaComponent->SetAiming(false);
            OnAim(false);
        }

        if (IsValid(CurrentWeapon))
        {
            CurrentWeapon->StopFire();
            if (CurrentWeapon->IsReloading()) CurrentWeapon->CancelReload();
            CurrentWeapon->SetActorHiddenInGame(true);
        }

        if (UHamaAnimInstance* HamaAnim = Cast<UHamaAnimInstance>(AnimInstance))
        {
            HamaAnim->SetEquippedWeapon(nullptr);
        }
    }
    else
    {
        if (IsValid(CurrentWeapon))
        {
            const bool bIsThrowing = (ThrowableComponent && ThrowableComponent->IsThrowingInProcess());
            CurrentWeapon->SetActorHiddenInGame(bIsThrowing);
            AttachWeaponToMesh(CurrentWeapon);
        }

        UpdateAnimInstanceWeapon();
    }
}

bool AHama::IsMeleeing() const
{
    UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
    if (!AnimInstance) return false;

    return (MeleeAttackMontage && AnimInstance->Montage_IsPlaying(MeleeAttackMontage)) ||
        (BrawlerAttackMontage && AnimInstance->Montage_IsPlaying(BrawlerAttackMontage));
}

void AHama::MeleeActionPressed()
{
    if (IsMeleeing() || IsDrinkingPerk() || IsDowned()) return;
    if (ThrowableComponent && ThrowableComponent->IsThrowingInProcess()) return;

    UAnimMontage* TargetMontage = GetActiveMeleeMontage();
    if (!TargetMontage) return;

    // تەنها لە Meleeـی ئاساییدا سپرینت دەبڕین؛ لە دۆخی Brawler یاریزان دەبێت بتوانێت بە ڕاکردنەوە هێرش بکات!
    if (!IsBrawlerModeActive() && IsSprinting())
    {
        StopSprint();
    }

    if (CurrentWeapon)
    {
        CurrentWeapon->StopFire();
        if (CurrentWeapon->IsReloading()) CurrentWeapon->CancelReload();
    }

    if (bIsAimButtonHold && HamaComponent)
    {
        HamaComponent->SetAiming(false);
        OnAim(false);
    }

    UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
    if (!AnimInstance) return;

    PlayMeleeMontageByCounter(TargetMontage);

    FOnMontageBlendingOutStarted BlendingOutDelegate;
    BlendingOutDelegate.BindUObject(this, &AHama::OnMeleeMontageBlendingOut);
    AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate, TargetMontage);

    Server_ExecuteMelee();
}

void AHama::OnMeleeMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
    if (Montage == MeleeAttackMontage || Montage == BrawlerAttackMontage)
    {
        if (!bInterrupted && bIsAimButtonHold && !IsBrawlerModeActive() && HamaComponent)
        {
            HamaComponent->SetAiming(true);
            OnAim(true);
        }
    }
}

void AHama::Server_ExecuteMelee_Implementation()
{
    if (IsDrinkingPerk() || IsDowned()) return;
    if (ThrowableComponent && ThrowableComponent->IsThrowingInProcess()) return;

    const bool bIsBrawler = IsBrawlerModeActive();
    const float ActiveCooldown = bIsBrawler ? HamaAbilityComponent->GetBrawlerCooldown() : MeleeCooldown;

    const float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastServerMeleeTime < (ActiveCooldown * 0.8f))
    {
        return;
    }

    LastServerMeleeTime = CurrentTime;
    bServerMeleeHitWindowOpen = true; 

    if (!bIsBrawler && IsSprinting()) StopSprint();
    if (CurrentWeapon)
    {
        CurrentWeapon->StopFire();
        if (CurrentWeapon->IsReloading()) CurrentWeapon->CancelReload();
    }

    if (GetNetMode() != NM_DedicatedServer && !IsLocallyControlled())
    {
        PlayMeleeMontageByCounter(GetActiveMeleeMontage());
    }

    MeleeCounter++;
    MARK_PROPERTY_DIRTY_FROM_NAME(AHama, MeleeCounter, this);
}

void AHama::OnRep_MeleeCounter()
{
    if (!IsLocallyControlled())
    {
        PlayMeleeMontageByCounter(GetActiveMeleeMontage());
    }
} 

void AHama::PerformMeleeHitDetection()
{
    if (!IsLocallyControlled() || !GetMesh()) return;

    const bool bIsBrawler = IsBrawlerModeActive();
    const float ActiveRange = bIsBrawler ? HamaAbilityComponent->GetBrawlerRange() : MeleeRange;
    const float ActiveRadius = bIsBrawler ? HamaAbilityComponent->GetBrawlerRadius() : MeleeRadius;

    const FVector TraceStart = bIsBrawler
        ? (GetActorLocation() + FVector(0.f, 0.f, 50.f))
        : GetMesh()->GetSocketLocation(MeleeSocketName);

    const FVector ForwardVector = GetBaseAimRotation().Vector();
    const FVector TraceEnd = TraceStart + (ForwardVector * ActiveRange);

    FCollisionQueryParams Params(SCENE_QUERY_STAT(MeleeTrace), false);
    Params.AddIgnoredActor(this);
    if (CurrentWeapon) Params.AddIgnoredActor(CurrentWeapon);

    FHitResult HitResult;
    const FCollisionShape SphereShape = FCollisionShape::MakeSphere(ActiveRadius);

    const bool bHit = GetWorld()->SweepSingleByChannel(
        HitResult,
        TraceStart,
        TraceEnd,
        FQuat::Identity,
        ECC_Melee,
        SphereShape,
        Params
    );

    if (bHit && IsValid(HitResult.GetActor()))
    {
        IDamageableInterface* DamageableActor = Cast<IDamageableInterface>(HitResult.GetActor());
        if (DamageableActor && DamageableActor->CanReceiveWeaponDamage())
        {
            if (HasAuthority())
            {
                bServerMeleeHitWindowOpen = false;
                ApplyMeleeDamageInternal(HitResult.GetActor(), HitResult.ImpactPoint);
            }
            else
            {
                Server_ValidateMeleeHit(HitResult.GetActor(), HitResult.ImpactPoint);
            }
        }
    }
}

void AHama::Server_ValidateMeleeHit_Implementation(AActor* HitActor, FVector_NetQuantize HitLocation)
{
    if (!bServerMeleeHitWindowOpen || !IsValid(HitActor))
    {
        return;
    }

    const bool bIsBrawler = IsBrawlerModeActive();
    const float ActiveCooldown = bIsBrawler ? HamaAbilityComponent->GetBrawlerCooldown() : MeleeCooldown;
    const float ActiveRange = bIsBrawler ? HamaAbilityComponent->GetBrawlerRange() : MeleeRange;
    const float ActiveRadius = bIsBrawler ? HamaAbilityComponent->GetBrawlerRadius() : MeleeRadius;

    const float CurrentTime = GetWorld()->GetTimeSeconds();
    if ((CurrentTime - LastServerMeleeTime) > (ActiveCooldown + 0.8f))
    {
        bServerMeleeHitWindowOpen = false;
        return;
    }

    IDamageableInterface* Damageable = Cast<IDamageableInterface>(HitActor);
    if (!Damageable || !Damageable->CanReceiveWeaponDamage())
    {
        return;
    }

    const FVector PlayerLoc = GetActorLocation();
    const float MaxAllowedDistanceSq = FMath::Square(ActiveRange + ActiveRadius + 100.f);
    const float ActualDistSq = FVector::DistSquared(PlayerLoc, HitLocation);

    if (ActualDistSq > MaxAllowedDistanceSq)
    {
        return;
    }

    FHitResult WallHit;
    FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(MeleeLoS), false, this);
    TraceParams.AddIgnoredActor(HitActor);
    if (CurrentWeapon) TraceParams.AddIgnoredActor(CurrentWeapon);

    const FVector EyeLocation = PlayerLoc + FVector(0.f, 0.f, 50.f);

    const bool bBlockedByWall = GetWorld()->LineTraceSingleByChannel(
        WallHit,
        EyeLocation,
        HitLocation,
        ECC_WorldStatic,
        TraceParams
    );

    if (bBlockedByWall)
    {
        return;
    }

    bServerMeleeHitWindowOpen = false;

    ApplyMeleeDamageInternal(HitActor, HitLocation);
}

void AHama::ApplyMeleeDamageInternal(AActor* HitActor, const FVector& HitLocation)
{
    const float FinalDamage = IsBrawlerModeActive() ? HamaAbilityComponent->GetBrawlerDamage() : MeleeDamage;

    FPointDamageEvent PointDamageEvent;
    PointDamageEvent.Damage = FinalDamage;

    const FVector HitDir = (HitLocation - GetActorLocation()).GetSafeNormal();
    PointDamageEvent.HitInfo = FHitResult(HitActor, nullptr, HitLocation, HitDir);
    PointDamageEvent.ShotDirection = HitDir;
    PointDamageEvent.DamageTypeClass = UMeleeDamageType::StaticClass();

    HitActor->TakeDamage(FinalDamage, PointDamageEvent, GetController(), this);
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
    if (!IsValid(Player) || Player == this || !Player->HamaComponent)
    {
        return false;
    }

    return !Player->HamaComponent->IsDowned() && !Player->bIsDead && !Player->GetDeathMachine();
}

void AHama::Server_BeginRevive_Implementation(AHama* DownedPlayer)
{
    if (!IsValid(DownedPlayer) || DownedPlayer == this || !DownedPlayer->HealthComponent) return;
    if (bIsDead || IsDowned() || DownedPlayer->bIsDead || !DownedPlayer->HealthComponent->IsDowned()) return;
    if (DownedPlayer->HealthComponent->IsBeingRevived()) return;

    const float EffectiveInteractDist = FMath::Max(SetIntractDistance, 200.0f);
    const float MaxAllowedDistSq = FMath::Square(EffectiveInteractDist + 100.0f);
    if (FVector::DistSquared(GetActorLocation(), DownedPlayer->GetActorLocation()) > MaxAllowedDistSq)
    {
        return;
    }

    FHitResult Hit;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    QueryParams.AddIgnoredActor(DownedPlayer);

    TArray<AActor*> AttachedActors;
    GetAttachedActors(AttachedActors);
    QueryParams.AddIgnoredActors(AttachedActors);

    AttachedActors.Reset();
    DownedPlayer->GetAttachedActors(AttachedActors);
    QueryParams.AddIgnoredActors(AttachedActors);

    const FVector TargetTraceLocation = DownedPlayer->GetActorLocation() + FVector(0.0f, 0.0f, 30.0f);
    if (GetWorld()->LineTraceSingleByChannel(Hit, GetPawnViewLocation(), TargetTraceLocation, ECC_Visibility, QueryParams))
    {
        if (Hit.GetActor() && !Hit.GetActor()->IsA<APawn>())
        {
            return;
        }
    }

    ClearAllReviveTimers();

    float FullReviveTime = (DefaultReviveTime > 0.0f) ? DefaultReviveTime : 4.0f;

    if (HasQuickRevive())
    {
        FullReviveTime *= 0.5f;
    }

    if (HamaAbilityComponent && HamaAbilityComponent->GetAssignedAbility() == EHamaAbilityType::MedicalSupport)
    {
        FullReviveTime *= 0.5f;
    }

    const float ExistingProgress = DownedPlayer->HealthComponent->GetActiveRevivePercent();
    const float RemainingFraction = FMath::Clamp(1.0f - ExistingProgress, 0.02f, 1.0f);
    const float TimeToComplete = FullReviveTime * RemainingFraction;

    AGameStateBase* GS = GetWorld()->GetGameState();

    const float ServerTime = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
    const float CalculatedEndTime = ServerTime + TimeToComplete;

    CurrentReviveTarget = DownedPlayer;
    DownedPlayer->HealthComponent->SetBeingRevived(true, this, FullReviveTime, false);

    ReviveTotalDuration = FullReviveTime;
    ReviveEndTime = CalculatedEndTime;

    MARK_PROPERTY_DIRTY_FROM_NAME(AHama, ReviveTotalDuration, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AHama, ReviveEndTime, this);

    if (IsLocallyControlled())
    {
        OnRep_ReviveData();
    }

    FTimerDelegate ReviveDel;
    ReviveDel.BindUObject(this, &AHama::Server_CompleteRevive);
    GetWorldTimerManager().SetTimer(ReviveTimerHandle, ReviveDel, TimeToComplete, false);

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
        CancelReviveInternal();
        return;
    }

    AHama* Target = CurrentReviveTarget.Get();
    if (!IsValid(Target) || Target->bIsDead || !Target->HealthComponent || !Target->HealthComponent->IsDowned())
    {
        CancelReviveInternal();
        return;
    }

    const float EffectiveInteractDist = FMath::Max(SetIntractDistance, 200.0f);
    const float MaxAllowedDistSq = FMath::Square(EffectiveInteractDist + 100.0f);
    if (FVector::DistSquared(GetActorLocation(), Target->GetActorLocation()) > MaxAllowedDistSq)
    {
        CancelReviveInternal();
        return;
    }
}

void AHama::Server_CancelRevive_Implementation()
{
    CancelReviveInternal();
}

void AHama::CancelReviveInternal()
{
    ClearAllReviveTimers();
}

void AHama::ClearAllReviveTimers()
{
    GetWorldTimerManager().ClearTimer(ReviveTimerHandle);
    GetWorldTimerManager().ClearTimer(ReviveCheckTimerHandle);

    const bool bWasReviving = (ReviveTotalDuration > 0.0f);

    ReviveEndTime = 0.0f;
    ReviveTotalDuration = 0.0f;
    MARK_PROPERTY_DIRTY_FROM_NAME(AHama, ReviveEndTime, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AHama, ReviveTotalDuration, this);

    if (AHama* Target = CurrentReviveTarget.Get())
    {
        if (Target->HealthComponent && Target->HealthComponent->GetCurrentReviver() == this)
        {
            Target->HealthComponent->SetBeingRevived(false);
        }
    }

    CurrentReviveTarget = nullptr;

    if (bWasReviving && IsLocallyControlled())
    {
        OnRep_ReviveData();
    }
}

void AHama::OnRep_ReviveData()
{
    if (HealthComponent)
    {
        const bool bIsRevivingOthers = (ReviveTotalDuration > 0.0f && ReviveEndTime > 0.0f);
        HealthComponent->OnReviveStateChanged.Broadcast(HealthComponent->IsBeingRevived(), bIsRevivingOthers);
    }
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
    if (IsValid(Target) && !Target->bIsDead && Target->HealthComponent && Target->HealthComponent->IsDowned())
    {
        Target->HealthComponent->Revive();

        if (AHamaPlayerState* MyPS = GetPlayerState<AHamaPlayerState>())
        {
            MyPS->AddPoints(100);
        }
    }

    ClearAllReviveTimers();
}

void AHama::SetIsDead(bool bNewIsDead)
{
    bIsDead = bNewIsDead;
    MARK_PROPERTY_DIRTY_FROM_NAME(AHama, bIsDead, this);
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

void AHama::Input_ThrowGrenadePressed()
{
    if (!ThrowableComponent) return;
    if(ThrowableComponent->IsThrowingInProcess())  return;
    if (IsDrinkingPerk()) return;
    if (IsMeleeing()) return;
    if (IsDiving()) return;
    if (IsDowned() || bIsDead) return;
    if (IsSprinting()) StopSprint();
       
    ThrowableComponent->StartGrenadeCharge();
}

void AHama::Input_ThrowGrenadeReleased()
{
    if (!ThrowableComponent) return;
    if (IsDrinkingPerk()) return;
    if (IsMeleeing()) return;
    if (IsDiving()) return;
    if (IsDowned() || bIsDead) return;

    ThrowableComponent->ReleaseGrenadeThrow();
}

void AHama::Input_ThrowMonkeyPressed()
{
    if (!ThrowableComponent) return;
    if (ThrowableComponent->IsThrowingInProcess())  return;
    if (IsDrinkingPerk()) return;
    if (IsMeleeing()) return;
    if (IsDiving()) return;
    if (IsDowned() || bIsDead) return;
    if (IsSprinting()) StopSprint();
    
    ThrowableComponent->StartMonkeyCharge();
}

void AHama::Input_ThrowMonkeyReleased()
{
    if (!ThrowableComponent) return;
    if (IsDrinkingPerk()) return;
    if (IsMeleeing()) return;
    if (IsDiving()) return;
    if (IsDowned() || bIsDead) return;
    
    ThrowableComponent->ReleaseMonkeyThrow();
}

bool AHama::SetCanInteract() const
{
   if(bIsDeathMachineActive || bIsDead || IsDowned() || IsDrinkingPerk() || IsMeleeing())
   {
       return false;
   }

   if (ThrowableComponent && ThrowableComponent->IsThrowingInProcess())
   {
       return false;
   }
   
   return true;
}

void AHama::Client_AnnouncePersonalPowerUp_Implementation(EPowerUpType PowerUpType)
{
    if (OnPersonalPowerUpAcquiredDelegate.IsBound())
    {
        OnPersonalPowerUpAcquiredDelegate.Broadcast(PowerUpType);
    }
}