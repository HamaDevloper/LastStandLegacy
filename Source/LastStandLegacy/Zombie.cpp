#include "Zombie.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "LastStandLegacyGameState.h"
#include "HamaPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "MeleeDamageType.h"
#include "ZombieDirectorSubsystem.h"
#include "Engine/AssetManager.h"
#include "Engine/DamageEvents.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"

AZombie::AZombie()
{
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;

    bReplicates = true;
    SetReplicateMovement(true);

    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    SetNetUpdateFrequency(10.f);
    SetMinNetUpdateFrequency(2.f);
    SetNetCullDistanceSquared(1600000000.0f);

    MaxHealth = BaseHealth;
    Health = MaxHealth;

    AttackDistanceSq = FMath::Square(AttackDistance);

    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCanEverAffectNavigation(false);
        Capsule->SetGenerateOverlapEvents(false);
    }

    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        MeshComp->bComponentUseFixedSkelBounds = true;
    }
}

void AZombie::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    CachedAIController = Cast<AAIController>(NewController);
}

void AZombie::BeginPlay()
{
    Super::BeginPlay();

    CachedMovement = GetCharacterMovement();
   
    if (CachedMovement)
    {
        CachedMovement->bEnablePhysicsInteraction = false;
        CachedMovement->bUseRVOAvoidance = false;

        CachedMovement->bSweepWhileNavWalking = false;
        CachedMovement->bAlwaysCheckFloor = false; 

        CachedMovement->bPushForceScaledToMass = false;
        CachedMovement->bScalePushForceToVelocity = false;

       
        CachedMovement->MaxStepHeight = 35.f; 
        CachedMovement->SetWalkableFloorAngle(45.f);

        CachedMovement->bOrientRotationToMovement = true;
        CachedMovement->RotationRate = FRotator(0.f, 540.f, 0.f);
        CachedMovement->NetworkSimulatedSmoothLocationTime = 0.06f;
        CachedMovement->NetworkSimulatedSmoothRotationTime = 0.03f;
    }

    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        CachedCapsuleCollisionEnabled = Capsule->GetCollisionEnabled();
    }

    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        CachedMeshRelativeLocation = MeshComp->GetRelativeLocation();
        CachedMeshRelativeRotation = MeshComp->GetRelativeRotation();
        CachedMeshCollisionProfile = MeshComp->GetCollisionProfileName();
        CachedMeshCollisionEnabled = MeshComp->GetCollisionEnabled();

        if (!IsRunningDedicatedServer())
        {
            MeshComp->bEnableUpdateRateOptimizations = true;
            MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
            MeshComp->bCastFarShadow = false;
            MeshComp->SetGenerateOverlapEvents(false);
        }
        else 
        {
            MeshComp->bNoSkeletonUpdate = true;
            MeshComp->SetComponentTickEnabled(false);
            MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
        }
    }

    if (!HasAuthority()) return;

    CachedGS = GetWorld()->GetGameState<ALastStandLegacyGameState>();
    CachedDirector = GetWorld()->GetSubsystem<UZombieDirectorSubsystem>();

    if (!MeshToSelect.IsEmpty())
    {
        MeshIndexSelected = static_cast<uint8>(FMath::RandRange(0, MeshToSelect.Num() - 1));
        MARK_PROPERTY_DIRTY_FROM_NAME(AZombie, MeshIndexSelected, this);
        ApplySelectedMesh();
    }
}

void AZombie::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(TimerHandle_ReturnToPool);
        World->GetTimerManager().ClearTimer(TimerHandle_RevealMesh);
    }

    if (HasAuthority() && bRegisteredWithDirector && CachedDirector)
    {
        CachedDirector->UnregisterZombie(this);
        bRegisteredWithDirector = false;
    }

    Super::EndPlay(EndPlayReason);
}

void AZombie::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Params;
    Params.bIsPushBased = true;

    DOREPLIFETIME_WITH_PARAMS_FAST(AZombie, bIsDead, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AZombie, PoolState, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AZombie, CurrentTarget, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AZombie, MeshIndexSelected, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AZombie, MaxHealth, Params);
}

void AZombie::OnRep_ReplicatedMovement()
{
    Super::OnRep_ReplicatedMovement();

    if (CachedMovement)
    {
        CachedMovement->bJustTeleported = true;
    }
}

void AZombie::InitializeForPool()
{
    if (!HasAuthority()) return;

    GetWorldTimerManager().ClearTimer(TimerHandle_RevealMesh);

    bIsDead = true;
    MARK_PROPERTY_DIRTY_FROM_NAME(AZombie, bIsDead, this);

    PoolState = EZombiePoolState::Pooled;
    MARK_PROPERTY_DIRTY_FROM_NAME(AZombie, PoolState, this);

    CurrentTarget = nullptr;
    MARK_PROPERTY_DIRTY_FROM_NAME(AZombie, CurrentTarget, this);

    if (CachedAIController)
    {
        CachedAIController->StopMovement();
        CachedAIController->ClearFocus(EAIFocusPriority::Gameplay);
    }

    if (CachedMovement)
    {
        CachedMovement->StopMovementImmediately();
        CachedMovement->Velocity = FVector::ZeroVector;
        CachedMovement->DisableMovement();
    }

    SetReplicateMovement(false);
    SetNetUpdateFrequency(1.f);

    ApplyStateVisualsAndPhysics(EZombiePoolState::Pooled);
}

void AZombie::ActivateFromPool(const FVector& SpawnLocation, const FRotator& SpawnRotation, int32 CurrentRound)
{
    if (!HasAuthority()) return;

    GetWorldTimerManager().ClearTimer(TimerHandle_ReturnToPool);
    GetWorldTimerManager().ClearTimer(TimerHandle_RevealMesh);

    CurrentTarget = nullptr;
    MARK_PROPERTY_DIRTY_FROM_NAME(AZombie, CurrentTarget, this);
    LastTargetLocation = FVector::ZeroVector;
    NextTargetSearchTime = 0.f;
    NextForceRepathTime = 0.f;

    if (CachedMovement)
    {
        CachedMovement->StopMovementImmediately();
        CachedMovement->Velocity = FVector::ZeroVector;
    }

    FVector FinalSpawnLocation = SpawnLocation;
    const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;

    FHitResult HitFloor;
    FVector TraceStart = SpawnLocation + FVector(0.f, 0.f, 50.f);
    FVector TraceEnd = SpawnLocation - FVector(0.f, 0.f, 300.f);
    FCollisionQueryParams FloorParams(SCENE_QUERY_STAT(ZombieFloorSnap), false, this);

    if (GetWorld()->LineTraceSingleByChannel(HitFloor, TraceStart, TraceEnd, ECC_WorldStatic, FloorParams))
    {
        FinalSpawnLocation = HitFloor.ImpactPoint + FVector(0.f, 0.f, HalfHeight + 2.f);
    }

    SetActorLocationAndRotation(FinalSpawnLocation, SpawnRotation, false, nullptr, ETeleportType::TeleportPhysics);

    if (CachedMovement)
    {
        CachedMovement->bJustTeleported = true;
        CachedMovement->SetMovementMode(MOVE_Walking);
    }

    SetStatsForRound(CurrentRound);

    bIsDead = false;
    MARK_PROPERTY_DIRTY_FROM_NAME(AZombie, bIsDead, this);

    PoolState = EZombiePoolState::Alive;
    MARK_PROPERTY_DIRTY_FROM_NAME(AZombie, PoolState, this);

    SetReplicateMovement(true);
    SetNetUpdateFrequency(16.f);

    ApplyStateVisualsAndPhysics(EZombiePoolState::Alive);

    if (CachedDirector && !bRegisteredWithDirector)
    {
        CachedDirector->RegisterZombie(this);
        bRegisteredWithDirector = true;
    }

    ForceNetUpdate();
}

void AZombie::ReturnToPool()
{
    if (!HasAuthority()) return;

    InitializeForPool();

    if (CachedDirector)
    {
        CachedDirector->ReturnZombieToPool(this);
    }

    ForceNetUpdate();
}

void AZombie::OnRep_PoolState()
{
    bIsDead = (PoolState != EZombiePoolState::Alive);
    ApplyStateVisualsAndPhysics(PoolState);
}

void AZombie::OnRep_IsDead()
{
    // بۆ پاراستنی گونجاوی ئەگەر بلوپرینتێک پشتی پێ بەستبێت
}

void AZombie::ApplyStateVisualsAndPhysics(EZombiePoolState NewState)
{
    UCapsuleComponent* Capsule = GetCapsuleComponent();
    USkeletalMeshComponent* CharacterMesh = GetMesh();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(TimerHandle_RevealMesh);
    }

    if (NewState == EZombiePoolState::Pooled)
    {
        SetActorHiddenInGame(true);

        if (Capsule)
        {
            Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }

        if (CharacterMesh)
        {
            if (!IsRunningDedicatedServer())
            {
                CharacterMesh->SetSimulatePhysics(false);
                CharacterMesh->ResetAllBodiesSimulatePhysics();
                CharacterMesh->PutAllRigidBodiesToSleep();
            }
            CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            CharacterMesh->AttachToComponent(Capsule, FAttachmentTransformRules::KeepRelativeTransform);
            CharacterMesh->SetRelativeLocationAndRotation(CachedMeshRelativeLocation, CachedMeshRelativeRotation);
        }
    }
    else if (NewState == EZombiePoolState::Alive)
    {
        SetActorHiddenInGame(true);

        if (Capsule)
        {
            Capsule->SetCollisionEnabled(CachedCapsuleCollisionEnabled);
        }

        if (CharacterMesh)
        {
            if (!IsRunningDedicatedServer())
            {
                CharacterMesh->SetSimulatePhysics(false);
                CharacterMesh->ResetAllBodiesSimulatePhysics();
                CharacterMesh->PutAllRigidBodiesToSleep();
                CharacterMesh->SetCollisionProfileName(CachedMeshCollisionProfile);
                CharacterMesh->SetCollisionEnabled(CachedMeshCollisionEnabled);

                if (UAnimInstance* AnimInst = CharacterMesh->GetAnimInstance())
                {
                    AnimInst->StopAllMontages(0.f);
                }
            }
            else
            {
                CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            }

            CharacterMesh->AttachToComponent(Capsule, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
            CharacterMesh->SetRelativeLocationAndRotation(CachedMeshRelativeLocation, CachedMeshRelativeRotation);

            if (!IsRunningDedicatedServer())
            {
                CharacterMesh->RefreshBoneTransforms();
                CharacterMesh->UpdateComponentToWorld();
            }
        }

        if (CachedMovement)
        {
            CachedMovement->bJustTeleported = true;
            if (!HasAuthority())
            {
                CachedMovement->ResetPredictionData_Client();
            }
        }

        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(
                TimerHandle_RevealMesh,
                this,
                &AZombie::FinalizePoolActivationVisuals,
                0.04f,
                false
            );
        }
    }
    else if (NewState == EZombiePoolState::Dead)
    {
        SetActorHiddenInGame(false);

        if (Capsule)
        {
            Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }

        if (CharacterMesh)
        {
            if (!IsRunningDedicatedServer())
            {
                CharacterMesh->SetCollisionProfileName(TEXT("Ragdoll"));
                CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                CharacterMesh->SetSimulatePhysics(true);
            }
            else
            {
                CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            }
        }
    }
}

void AZombie::FinalizePoolActivationVisuals()
{
    if (PoolState != EZombiePoolState::Alive || bIsDead) return;

    if (CachedMovement)
    {
        CachedMovement->bJustTeleported = true;
        if (!HasAuthority())
        {
            CachedMovement->ResetPredictionData_Client();
        }
    }

    if (USkeletalMeshComponent* CharacterMesh = GetMesh())
    {
        CharacterMesh->SetRelativeLocationAndRotation(CachedMeshRelativeLocation, CachedMeshRelativeRotation);
        if (!IsRunningDedicatedServer())
        {
            CharacterMesh->RefreshBoneTransforms();
            CharacterMesh->UpdateComponentToWorld();
        }
    }

    SetActorHiddenInGame(false);
}

void AZombie::SetStatsForRound(int32 CurrentRound)
{
    if (!HasAuthority()) return;

    const float NewHealth = BaseHealth * FMath::Pow(1.12f, CurrentRound - 1);
    const float ClampedMaxHealth = FMath::Clamp(NewHealth, BaseHealth, MaxHealthZombieReach);

    if (!FMath::IsNearlyEqual(MaxHealth, ClampedMaxHealth))
    {
        MaxHealth = ClampedMaxHealth;
        MARK_PROPERTY_DIRTY_FROM_NAME(AZombie, MaxHealth, this);
    }

    Health = MaxHealth;

    const float Alpha = FMath::Clamp((CurrentRound - 1) / 19.f, 0.f, 1.f);
    const float BaseSpeed = FMath::Lerp(MinWalkSpeed, MaxBaseWalkSpeed, Alpha);

    constexpr float TierOffsets[] = { -40.f, -20.f, 0.f, 30.f, 70.f };
    const int32 MaxTier = FMath::Clamp(CurrentRound, 1, 5);
    const int32 RandTier = FMath::RandRange(0, MaxTier - 1);

    if (CachedMovement)
    {
        CachedMovement->MaxWalkSpeed = FMath::Clamp(
            BaseSpeed + TierOffsets[RandTier],
            MinWalkSpeed,
            AbsoluteMaxSpeed);
    }
}

void AZombie::OnRep_SelectedIndex()
{
    ApplySelectedMesh();
}

void AZombie::ApplySelectedMesh()
{
    if (!MeshToSelect.IsValidIndex(MeshIndexSelected)) return;

    TSoftObjectPtr<USkeletalMesh> SoftMesh = MeshToSelect[MeshIndexSelected];

    if (USkeletalMesh* LoadedMesh = SoftMesh.Get())
    {
        if (USkeletalMeshComponent* MeshComp = GetMesh())
        {
            MeshComp->SetSkeletalMeshAsset(LoadedMesh);
        }
        return;
    }

    FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
    TWeakObjectPtr<AZombie> WeakThis(this);

    Streamable.RequestAsyncLoad(
        SoftMesh.ToSoftObjectPath(),
        [WeakThis, SoftMesh]()
        {
            if (AZombie* StrongThis = WeakThis.Get())
            {
                if (USkeletalMesh* AsyncLoadedMesh = SoftMesh.Get())
                {
                    if (USkeletalMeshComponent* MeshComp = StrongThis->GetMesh())
                    {
                        MeshComp->SetSkeletalMeshAsset(AsyncLoadedMesh);
                    }
                }
            }
        }
    );
}

void AZombie::ExecuteMeleeHit()
{
    if (!HasAuthority() || bIsDead || !CurrentTarget) return;

    const float DistSq = FVector::DistSquared(GetActorLocation(), CurrentTarget->GetActorLocation());

    if (DistSq <= FMath::Square(AttackDistance + 50.f))
    {
        UGameplayStatics::ApplyDamage(
            CurrentTarget,
            AttackDamage,
            GetController(),
            this,
            UDamageType::StaticClass());
    }
}

float AZombie::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (!HasAuthority() || bIsDead || PoolState != EZombiePoolState::Alive)
    {
        return 0.f;
    }

    float DamageApplied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    if (DamageApplied <= 0.f && DamageAmount > 0.f)
    {
        DamageApplied = DamageAmount;
    }

    const bool bIsMelee = DamageEvent.DamageTypeClass &&
        DamageEvent.DamageTypeClass->IsChildOf(UMeleeDamageType::StaticClass());

    bool bDoublePoints = false;
    if (CachedGS)
    {
        bDoublePoints = CachedGS->bIsDoublePointsActive;
        if (CachedGS->bHasInstaKill)
        {
            DamageApplied = Health;
        }
    }

    Health = FMath::Clamp(Health - DamageApplied, 0.f, MaxHealth);

    AHamaPlayerState* TargetPlayerState = EventInstigator ? EventInstigator->GetPlayerState<AHamaPlayerState>() : nullptr;

    if (Health <= 0.f)
    {
        if (TargetPlayerState)
        {
            const int32 Points = bIsMelee ? 130 : 100;
            TargetPlayerState->AddPoints(bDoublePoints ? Points * 2 : Points);
            TargetPlayerState->AddKills(1);
        }

        Die(EventInstigator);
    }
    else if (TargetPlayerState)
    {
        const int32 Points = bIsMelee ? 20 : 10;
        TargetPlayerState->AddPoints(bDoublePoints ? Points * 2 : Points);
    }

    return DamageApplied;
}

void AZombie::Die(AController* KillerController)
{
    if (bIsDead || !HasAuthority() || PoolState != EZombiePoolState::Alive)
    {
        return;
    }

    bIsDead = true;
    MARK_PROPERTY_DIRTY_FROM_NAME(AZombie, bIsDead, this);

    PoolState = EZombiePoolState::Dead;
    MARK_PROPERTY_DIRTY_FROM_NAME(AZombie, PoolState, this);

    CurrentTarget = nullptr;
    MARK_PROPERTY_DIRTY_FROM_NAME(AZombie, CurrentTarget, this);

    if (bRegisteredWithDirector && CachedDirector)
    {
        CachedDirector->UnregisterZombie(this);
        bRegisteredWithDirector = false;
    }

    if (CachedAIController)
    {
        CachedAIController->StopMovement();
    }

    if (CachedMovement)
    {
        CachedMovement->StopMovementImmediately();
        CachedMovement->DisableMovement();
    }

    SetReplicateMovement(false);

    ApplyStateVisualsAndPhysics(EZombiePoolState::Dead);

    OnZombieDeath.ExecuteIfBound(this, KillerController);

    ForceNetUpdate();

    GetWorldTimerManager().SetTimer(TimerHandle_ReturnToPool, this, &AZombie::ReturnToPool, 2.0f, false);
}