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

    if (CachedAIController)
    {
        CachedAIController->SetActorTickEnabled(false);
    }
}

void AZombie::BeginPlay()
{
    Super::BeginPlay();

    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        const ECollisionEnabled::Type CurrentCapCol = Capsule->GetCollisionEnabled();
        CachedCapsuleCollisionEnabled = (CurrentCapCol != ECollisionEnabled::NoCollision)
            ? CurrentCapCol
            : ECollisionEnabled::QueryAndPhysics;
    }

    CachedMovement = GetCharacterMovement();

    if (CachedMovement)
    {
        CachedMovement->bEnablePhysicsInteraction = false;
        CachedMovement->bSweepWhileNavWalking = false;
        CachedMovement->bAlwaysCheckFloor = false;
        CachedMovement->bPushForceScaledToMass = false;
        CachedMovement->bScalePushForceToVelocity = false;
        CachedMovement->MaxStepHeight = 35.f;
        CachedMovement->SetWalkableFloorAngle(45.f);
        CachedMovement->bOrientRotationToMovement = true;
        CachedMovement->RotationRate = FRotator(0.f, 540.f, 0.f);

        CachedMovement->MaxSimulationIterations = 4;
        CachedMovement->bUseFlatBaseForFloorChecks = true;

        CachedMovement->bUseRVOAvoidance = false;
        CachedMovement->SetAvoidanceEnabled(false);

        CachedMovement->NetworkSimulatedSmoothLocationTime = 0.06f;
        CachedMovement->NetworkSimulatedSmoothRotationTime = 0.03f;
    }

    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        CachedMeshRelativeLocation = MeshComp->GetRelativeLocation();
        CachedMeshRelativeRotation = MeshComp->GetRelativeRotation();
        CachedMeshCollisionProfile = MeshComp->GetCollisionProfileName();
        CachedMeshObjectType = MeshComp->GetCollisionObjectType();
        CachedMeshCollisionResponses = MeshComp->GetCollisionResponseToChannels();

        const ECollisionEnabled::Type CurrentMeshCol = MeshComp->GetCollisionEnabled();
        CachedMeshCollisionEnabled = (CurrentMeshCol != ECollisionEnabled::NoCollision)
            ? CurrentMeshCol
            : ECollisionEnabled::QueryOnly;

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
            MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
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
        World->GetTimerManager().ClearTimer(TimerHandle_Dormant);
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

    DOREPLIFETIME_WITH_PARAMS_FAST(AZombie, PoolState, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AZombie, MeshIndexSelected, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AZombie, MaxHealth, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AZombie, AttackPlayCount, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AZombie, SelectedAttackMontageIndex, Params);
}

void AZombie::ApplyServerLOD(uint8 Tier)
{
    if (Tier == ServerLODTier || !CachedMovement) return;
    ServerLODTier = Tier;

    static constexpr float NetFreq[] = { 12.f, 8.f, 4.f };
    static constexpr float MoveTickInt[] = { 0.f, 0.033f, 0.1f };

    SetNetUpdateFrequency(NetFreq[Tier]);
    CachedMovement->SetComponentTickInterval(MoveTickInt[Tier]);
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
    GetWorldTimerManager().ClearTimer(TimerHandle_Dormant);

    bIsDead = true;
    bIsAttacking = false;

    PoolState = EZombiePoolState::Pooled;
    MARK_PROPERTY_DIRTY_FROM_NAME(AZombie, PoolState, this);

    CurrentTarget = nullptr;

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

    if (IsRunningDedicatedServer())
    {
        if (USkeletalMeshComponent* MeshComp = GetMesh())
        {
            MeshComp->bNoSkeletonUpdate = true;
            MeshComp->SetComponentTickEnabled(false);
        }
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
    GetWorldTimerManager().ClearTimer(TimerHandle_Dormant);

    SetNetDormancy(DORM_Awake);

    bIsDead = false;
    bIsAttacking = false;
    CurrentTarget = nullptr;
    LastTargetLocation = FVector::ZeroVector;
    NextTargetSearchTime = 0.f;
    NextForceRepathTime = 0.f;
    NextRepathAllowedTime = 0.f;
    NextAllowedAttackTime = 0.f;
    ServerLODTier = 255;

    if (CachedMovement)
    {
        CachedMovement->StopMovementImmediately();
        CachedMovement->Velocity = FVector::ZeroVector;
        CachedMovement->SetComponentTickInterval(0.f);
    }

    const FVector2D RandCircle = FMath::RandPointInCircle(55.f);
    FVector AdjustedSpawn = SpawnLocation + FVector(RandCircle.X, RandCircle.Y, 0.f);

    const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;

    FHitResult HitFloor;
    const FVector TraceStart = AdjustedSpawn + FVector(0.f, 0.f, 60.f);
    const FVector TraceEnd = AdjustedSpawn - FVector(0.f, 0.f, 300.f);
    FCollisionQueryParams FloorParams(SCENE_QUERY_STAT(ZombieFloorSnap), false, this);

    if (GetWorld()->LineTraceSingleByChannel(HitFloor, TraceStart, TraceEnd, ECC_WorldStatic, FloorParams))
    {
        AdjustedSpawn = HitFloor.ImpactPoint + FVector(0.f, 0.f, HalfHeight + 2.f);
    }

    FVector FinalSpawnLocation = AdjustedSpawn;
    GetWorld()->FindTeleportSpot(this, FinalSpawnLocation, SpawnRotation);

    SetActorLocationAndRotation(FinalSpawnLocation, SpawnRotation, false, nullptr, ETeleportType::TeleportPhysics);

    PoolState = EZombiePoolState::Alive;
    MARK_PROPERTY_DIRTY_FROM_NAME(AZombie, PoolState, this);

    ApplyStateVisualsAndPhysics(EZombiePoolState::Alive);

    if (CachedMovement)
    {
        CachedMovement->bJustTeleported = true;
        CachedMovement->SetMovementMode(MOVE_Walking);
    }

    SetStatsForRound(CurrentRound);

    SetReplicateMovement(true);
    SetNetUpdateFrequency(16.f);

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

    // خستنە دۆخی Dormant دوای ناردنی پاکێتی کۆتایی بە ٠.٥ چرکە
    GetWorldTimerManager().SetTimer(TimerHandle_Dormant, this, &AZombie::GoDormant, 0.5f, false);
}

void AZombie::GoDormant()
{
    if (PoolState == EZombiePoolState::Pooled)
    {
        SetNetDormancy(DORM_DormantAll);
    }
}

void AZombie::OnRep_PoolState()
{
    bIsDead = (PoolState != EZombiePoolState::Alive);
    ApplyStateVisualsAndPhysics(PoolState);
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
            CharacterMesh->AttachToComponent(Capsule, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
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
            CharacterMesh->AttachToComponent(Capsule, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
            CharacterMesh->SetRelativeLocationAndRotation(CachedMeshRelativeLocation, CachedMeshRelativeRotation);

            if (!IsRunningDedicatedServer())
            {
                CharacterMesh->SetSimulatePhysics(false);
                CharacterMesh->ResetAllBodiesSimulatePhysics();
                CharacterMesh->PutAllRigidBodiesToSleep();

                if (CachedMeshCollisionProfile != UCollisionProfile::CustomCollisionProfileName && CachedMeshCollisionProfile != TEXT("Custom"))
                {
                    CharacterMesh->SetCollisionProfileName(CachedMeshCollisionProfile);
                }
                CharacterMesh->SetCollisionObjectType(CachedMeshObjectType);
                CharacterMesh->SetCollisionResponseToChannels(CachedMeshCollisionResponses);
                CharacterMesh->SetCollisionEnabled(CachedMeshCollisionEnabled);

                // RecreatePhysicsState بە تەواوی لابرا؛ هیچ پێویست نییە و دەبووە هۆی Frame Drop ی کڵایەنت

                if (UAnimInstance* AnimInst = CharacterMesh->GetAnimInstance())
                {
                    AnimInst->StopAllMontages(0.f);
                }
            }
            else
            {
                CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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

    SetActorHiddenInGame(false);

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

void AZombie::TryAttack()
{
    if (!HasAuthority() || bIsDead || bIsAttacking || !CurrentTarget) return;

    const float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime < NextAllowedAttackTime) return;

    // ئەگەر مۆنتاژ نەبوو، کاتی هێرشەکە بە فەرمی وەربگرە و ڕاستەوخۆ دێمەیج لێبدە (Fallback)
    if (AttackMontages.IsEmpty())
    {
        NextAllowedAttackTime = CurrentTime + AttackCooldown;
        ExecuteMeleeHit(); // ڕاستەوخۆ لێدانەکە تاقی دەکاتەوە
        return;
    }

    const int32 RandIndex = FMath::RandRange(0, AttackMontages.Num() - 1);
    UAnimMontage* SelectedMontage = AttackMontages[RandIndex];
    if (!SelectedMontage) return;

    USkeletalMeshComponent* MeshComp = GetMesh();
    UAnimInstance* AnimInst = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
    if (!AnimInst) return;

    if (IsRunningDedicatedServer())
    {
        MeshComp->bNoSkeletonUpdate = false;
        MeshComp->SetComponentTickEnabled(true);
    }

    const float Duration = AnimInst->Montage_Play(SelectedMontage);
    if (Duration <= 0.f)
    {
        if (IsRunningDedicatedServer())
        {
            MeshComp->bNoSkeletonUpdate = true;
            MeshComp->SetComponentTickEnabled(false);
        }
        return;
    }

    bIsAttacking = true;
    NextAllowedAttackTime = CurrentTime + AttackCooldown;

    if (CachedAIController)
    {
        CachedAIController->StopMovement();
    }

    FOnMontageEnded EndedDelegate;
    EndedDelegate.BindUObject(this, &AZombie::OnAttackMontageEnded);
    AnimInst->Montage_SetEndDelegate(EndedDelegate, SelectedMontage);

    SelectedAttackMontageIndex = static_cast<uint8>(RandIndex);
    MARK_PROPERTY_DIRTY_FROM_NAME(AZombie, SelectedAttackMontageIndex, this);

    AttackPlayCount++;
    MARK_PROPERTY_DIRTY_FROM_NAME(AZombie, AttackPlayCount, this);
}

void AZombie::OnRep_AttackPlayCount()
{
    if (AttackMontages.IsValidIndex(SelectedAttackMontageIndex))
    {
        if (UAnimMontage* MontageToPlay = AttackMontages[SelectedAttackMontageIndex])
        {
            if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
            {
                AnimInst->Montage_Play(MontageToPlay);
            }
        }
    }
}

void AZombie::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (!HasAuthority()) return;

    bIsAttacking = false;

    // کوژاندنەوەی تیکی مێش پاش تەواوبوونی لێدان لەسەر سێرڤەر
    if (IsRunningDedicatedServer())
    {
        if (USkeletalMeshComponent* MeshComp = GetMesh())
        {
            MeshComp->bNoSkeletonUpdate = true;
            MeshComp->SetComponentTickEnabled(false);
        }
    }
}

void AZombie::ExecuteMeleeHit()
{
    if (!HasAuthority() || bIsDead) return;

    const float SphereRadius = 45.f;
    const FVector Forward = GetActorForwardVector();
    // دەستپێکردنی سفیرەکە کەمێک لە پێش کەپسولەکەوە تا لەگەڵ زۆمبییەکانی تەنیشتی تێکەڵ نەبێت
    const FVector Start = GetActorLocation() + (Forward * 30.f) + FVector(0.f, 0.f, 30.f);
    const FVector End = Start + (Forward * AttackDistance);

    TArray<FHitResult> HitResults;
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(SphereRadius);
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZombieMeleeHit), false, this);

    // بەکارهێنانی Multi بۆ ئەوەی ئەگەر کەپسولی زۆمبییەکی تر لە پێش بوو، تێپەڕێت بۆ سەر یاریزانەکە
    const bool bHitAny = GetWorld()->SweepMultiByChannel(
        HitResults,
        Start,
        End,
        FQuat::Identity,
        ECC_Pawn,
        SphereShape,
        QueryParams
    );

    AActor* ValidTargetHit = nullptr;
    FVector ImpactVisualPoint = End;

    if (bHitAny)
    {
        for (const FHitResult& Hit : HitResults)
        {
            AActor* HitActor = Hit.GetActor();
            // لێدان تەنیا ئەگەر ئەکتەرەکە یاریزان بێت و هاوپۆلی زۆمبی نەبێت
            if (HitActor && HitActor != this && !HitActor->IsA<AZombie>())
            {
                ValidTargetHit = HitActor;
                ImpactVisualPoint = Hit.ImpactPoint;
                break;
            }
        }
    }

#if ENABLE_DRAW_DEBUG
    const bool bDamagedPlayer = (ValidTargetHit != nullptr);
    const FColor TraceColor = bDamagedPlayer ? FColor::Green : FColor::Red;
    const float LifeTime = 1.5f;

    // ١. سفیری سەرەتایی بە بەکارهێنانی ڕادیۆسی ڕاست (SphereRadius)
    DrawDebugSphere(GetWorld(), Start, SphereRadius, 12, FColor::Yellow, false, LifeTime, 0, 1.5f);

    // ٢. سفیری کۆتایی
    DrawDebugSphere(GetWorld(), End, SphereRadius, 12, TraceColor, false, LifeTime, 0, 1.5f);

    // ٣. هێڵی ئاڕاستەی لێدان
    DrawDebugLine(GetWorld(), Start, End, TraceColor, false, LifeTime, 0, 2.0f);

    // ٤. نیشاندانی خاڵی بەرکەوتن
    if (bDamagedPlayer)
    {
        DrawDebugPoint(GetWorld(), ImpactVisualPoint, 15.f, FColor::Cyan, false, LifeTime);
    }
#endif

    if (ValidTargetHit)
    {
        UGameplayStatics::ApplyDamage(
            ValidTargetHit,
            AttackDamage,
            GetController(),
            this,
            UDamageType::StaticClass()
        );
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
    bIsAttacking = false;

    if (IsRunningDedicatedServer())
    {
        if (USkeletalMeshComponent* MeshComp = GetMesh())
        {
            MeshComp->bNoSkeletonUpdate = true;
            MeshComp->SetComponentTickEnabled(false);
        }
    }

    PoolState = EZombiePoolState::Dead;
    MARK_PROPERTY_DIRTY_FROM_NAME(AZombie, PoolState, this);

    CurrentTarget = nullptr;

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