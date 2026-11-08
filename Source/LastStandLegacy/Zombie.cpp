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

    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    SetNetUpdateFrequency(5.f);
    SetMinNetUpdateFrequency(5.f);
    SetNetCullDistanceSquared(1600000000.0f);

    MaxHealth = BaseHealth;
    Health = MaxHealth;

    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCanEverAffectNavigation(false);
        Capsule->SetGenerateOverlapEvents(false);
    }

    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        MeshComp->bComponentUseFixedSkelBounds = true;
    }

    AttackDistanceSq = FMath::Square(AttackDistance);
    AttackTriggerDistanceSq = FMath::Square(AttackTriggerDistance);
}

void AZombie::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    CachedAIController = Cast<AAIController>(NewController);

    if (CachedAIController)
    {
        CachedAIController->SetActorTickInterval(0.f);
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

    bUseControllerRotationYaw = false;

    CachedMovement = GetCharacterMovement();

    CachedMovement = GetCharacterMovement();

    if (CachedMovement)
    {
        // بەکارهێنانی MOVE_Walking وەک دۆخی بنەڕەتی
        CachedMovement->DefaultLandMovementMode = MOVE_Walking;

        // ١. سووککردنی تێچووی فیزیای زەوی و بەرکەوتن (CPU Optimization)
        CachedMovement->bEnablePhysicsInteraction = false;
        CachedMovement->bAlwaysCheckFloor = false;
        CachedMovement->bUseFlatBaseForFloorChecks = true;
        CachedMovement->bComponentShouldUpdatePhysicsVolume = false; // پشکنینی PhysicsVolume ناکات
        CachedMovement->bPushForceScaledToMass = false;
        CachedMovement->bScalePushForceToVelocity = false;
        CachedMovement->bEnablePhysicsInteraction = false;
        CachedMovement->MaxSimulationIterations = 2; // زۆر کەم-تێچووترە لە 4 و بۆ AI بەتەواوی بەسە

        CachedMovement->MaxStepHeight = 35.f;
        CachedMovement->SetWalkableFloorAngle(45.f);

        // ٢. ڕێکخستنی سوڕانەوە (Director بەپێی مەودا دەیکاتە bUseControllerDesiredRotation)
        CachedMovement->bOrientRotationToMovement = true;
        CachedMovement->bUseControllerDesiredRotation = false;
        CachedMovement->RotationRate = FRotator(0.f, 540.f, 0.f);

        // ٣. نەهێشتنی خۆلادانی ناوخۆیی (چونکە سیستەمی Director بەڕێوەی دەبات)
        CachedMovement->bUseRVOAvoidance = false;
        CachedMovement->SetAvoidanceEnabled(false);

        // ٤. نەرمکردنەوەی جووڵە لەسەر کللاینت بۆ ڕێگری لە لاگ و پچڕانی سوڕانەوە
        CachedMovement->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
        CachedMovement->NetworkSimulatedSmoothLocationTime = 0.11f;
        CachedMovement->NetworkSimulatedSmoothRotationTime = 0.09f;
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
    DOREPLIFETIME_WITH_PARAMS_FAST(AZombie, RepAttackPlayRateByte, Params);
}

void AZombie::ApplyServerLOD(uint8 Tier)
{
    if (Tier == ServerLODTier || !CachedMovement) return;
    ServerLODTier = Tier;

    // ٢٠ هێرتز بۆ Tier 0 (کەمتر لە ١٢ مەتر) تا سوڕانەوە و جووڵە لە نزیک یاریزانەکە زۆر نەرم بێت
    static constexpr float NetFreq[] = { 20.f, 10.f, 4.f };
    static constexpr float MoveTickInt[] = { 0.f, 0.033f, 0.08f };

    SetNetUpdateFrequency(NetFreq[Tier]);
    CachedMovement->SetComponentTickInterval(MoveTickInt[Tier]);
}

void AZombie::OnRep_ReplicatedMovement()
{
    const FVector OldLocation = GetActorLocation();
    Super::OnRep_ReplicatedMovement();

    if (CachedMovement)
    {
        if (FVector::DistSquared(OldLocation, GetActorLocation()) > FMath::Square(400.f))
        {
            CachedMovement->bJustTeleported = true;
            if (!HasAuthority())
            {
                CachedMovement->ResetPredictionData_Client();
            }
        }
    }
}

void AZombie::InitializeForPool()
{
    if (!HasAuthority()) return;

    GetWorldTimerManager().ClearTimer(TimerHandle_RevealMesh);
    GetWorldTimerManager().ClearTimer(TimerHandle_Dormant);

    bIsDead = true;
    bIsAttacking = false;

    // دڵنیابوونەوە لە دەرکردنی زۆمبییەکە لە Director تەنانەت ئەگەر بێ Die() بخرێتەوە ناو Pool
    if (bRegisteredWithDirector && CachedDirector)
    {
        CachedDirector->UnregisterZombie(this);
        bRegisteredWithDirector = false;
    }

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
        CachedMovement->bOrientRotationToMovement = true;
        CachedMovement->bUseControllerDesiredRotation = false;
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
        CachedMovement->bOrientRotationToMovement = true;
        CachedMovement->bUseControllerDesiredRotation = false;
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
    SetNetUpdateFrequency(20.f);

    if (CachedDirector && !bRegisteredWithDirector)
    {
        CachedDirector->RegisterZombie(this);
        bRegisteredWithDirector = true;
    }

    const float Now = GetWorld()->GetTimeSeconds();
    LastSpawnOrRelocateTime = Now;
    NextRecycleCheckTime = Now + 5.0f;

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
                CharacterMesh->SetHiddenInGame(true);
            }
            CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            CharacterMesh->AttachToComponent(Capsule, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
            CharacterMesh->SetRelativeLocationAndRotation(CachedMeshRelativeLocation, CachedMeshRelativeRotation);
        }
    }
    else if (NewState == EZombiePoolState::Alive)
    {
        // ئەکتەرەکە خۆی دیارە، تەنها مێشەکە بە شێوەی لۆکاڵی بۆ ٠.٠٤ چرکە دەشارینەوە تا کێشەی Replication نەبێت
        SetActorHiddenInGame(false);

        if (Capsule)
        {
            Capsule->SetCollisionEnabled(CachedCapsuleCollisionEnabled);
        }

        if (CharacterMesh)
        {
            if (!IsRunningDedicatedServer())
            {
                // زۆر گرنگ: پێش AttachToComponent دەبێت فیزیای Ragdoll بکوژێنرێتەوە!
                CharacterMesh->SetSimulatePhysics(false);
                CharacterMesh->ResetAllBodiesSimulatePhysics();
                CharacterMesh->PutAllRigidBodiesToSleep();
                CharacterMesh->SetHiddenInGame(true);
            }

            CharacterMesh->AttachToComponent(Capsule, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
            CharacterMesh->SetRelativeLocationAndRotation(CachedMeshRelativeLocation, CachedMeshRelativeRotation);

            if (!IsRunningDedicatedServer())
            {
                if (CachedMeshCollisionProfile != UCollisionProfile::CustomCollisionProfileName && CachedMeshCollisionProfile != TEXT("Custom"))
                {
                    CharacterMesh->SetCollisionProfileName(CachedMeshCollisionProfile);
                }
                CharacterMesh->SetCollisionObjectType(CachedMeshObjectType);
                CharacterMesh->SetCollisionResponseToChannels(CachedMeshCollisionResponses);
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
        }

        if (CachedMovement)
        {
            CachedMovement->bJustTeleported = true;
            if (!HasAuthority())
            {
                CachedMovement->ResetPredictionData_Client();
            }
        }

        if (!IsRunningDedicatedServer())
        {
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
                CharacterMesh->SetHiddenInGame(false);
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
            CharacterMesh->SetHiddenInGame(false);
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

    const float CombatAlpha = FMath::Clamp((CurrentRound - 1) / 9.f, 0.f, 1.f);
    CurrentAttackCooldown = FMath::Lerp(Round1AttackCooldown, HighRoundAttackCooldown, CombatAlpha);
    CurrentAttackPlayRate = FMath::Lerp(Round1AttackPlayRate, HighRoundAttackPlayRate, CombatAlpha);

    const uint8 NewPlayRateByte = static_cast<uint8>(FMath::RoundToInt(CurrentAttackPlayRate * 100.0f));
    if (RepAttackPlayRateByte != NewPlayRateByte)
    {
        RepAttackPlayRateByte = NewPlayRateByte;
        MARK_PROPERTY_DIRTY_FROM_NAME(AZombie, RepAttackPlayRateByte, this);
    }

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
    if (!HasAuthority() || bIsDead || !CurrentTarget || bIsAttacking) return;

    const float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime < NextAllowedAttackTime) return;

    // تێبینی: SetActorRotation لێرە لابرا، چونکە Director خۆی لە ڕێگەی SetFocus و bUseControllerDesiredRotation
    // بە نەرمی ڕووی زۆمبییەکە لە یاریزانەکە دەکات بێ ئەوەی لەسەر کللاینت تووشی سناپی کتوپڕ ببێت.

    if (AttackMontages.IsEmpty())
    {
        NextAllowedAttackTime = CurrentTime + CurrentAttackCooldown;
        ExecuteMeleeHit();
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

    const float Duration = AnimInst->Montage_Play(SelectedMontage, CurrentAttackPlayRate);
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
    NextAllowedAttackTime = CurrentTime + (Duration / FMath::Max(CurrentAttackPlayRate, 0.1f)) + CurrentAttackCooldown;

    FOnMontageEnded EndedDelegate;
    EndedDelegate.BindUObject(this, &AZombie::OnAttackMontageEnded);
    AnimInst->Montage_SetEndDelegate(EndedDelegate, SelectedMontage);

    SelectedAttackMontageIndex = static_cast<uint8>(RandIndex);
    MARK_PROPERTY_DIRTY_FROM_NAME(AZombie, SelectedAttackMontageIndex, this);

    AttackPlayCount++;
    MARK_PROPERTY_DIRTY_FROM_NAME(AZombie, AttackPlayCount, this);
}

void AZombie::OnRep_AttackPlayRate()
{
    CurrentAttackPlayRate = (RepAttackPlayRateByte > 0)
        ? (static_cast<float>(RepAttackPlayRateByte) / 100.0f)
        : 1.0f;
}

void AZombie::OnRep_AttackPlayCount()
{
    // دڵنیابوونەوە لە خێرایی ڕاست تەنانەت ئەگەر ئەم RepNotifyـە پێش OnRep_AttackPlayRate کار بکات
    if (RepAttackPlayRateByte > 0)
    {
        CurrentAttackPlayRate = static_cast<float>(RepAttackPlayRateByte) / 100.0f;
    }

    if (AttackMontages.IsValidIndex(SelectedAttackMontageIndex))
    {
        if (UAnimMontage* MontageToPlay = AttackMontages[SelectedAttackMontageIndex])
        {
            if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
            {
                AnimInst->Montage_Play(MontageToPlay, FMath::Max(CurrentAttackPlayRate, 0.1f));
            }
        }
    }
}

void AZombie::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (!HasAuthority()) return;

    bIsAttacking = false;

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

    UWorld* World = GetWorld();
    if (!World) return;

    const float SphereRadius = 45.f;
    const FVector Forward = GetActorForwardVector();
    const FVector Start = GetActorLocation() + (Forward * 30.f) + FVector(0.f, 0.f, 30.f);
    const FVector End = Start + (Forward * AttackDistance);

    TArray<FHitResult> HitResults;
    const FCollisionShape SphereShape = FCollisionShape::MakeSphere(SphereRadius);
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZombieMeleeHit), false, this);

    // بەکارهێنانی ObjectType لەبری Channel تا کەپسولی زۆمبییەکانی پێشەوە تیشکەکە نەبڕن!
    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

    const bool bHitAny = World->SweepMultiByObjectType(
        HitResults,
        Start,
        End,
        FQuat::Identity,
        ObjectParams,
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
            if (HitActor && HitActor != this && !HitActor->IsA<AZombie>())
            {
                // پشکنینی دیوار تا زۆمبی لەودیو دیوار یان دەرگای داخراوەوە لێدان نەدات لە یاریزان
                FCollisionQueryParams LosParams(SCENE_QUERY_STAT(ZombieMeleeLOS), false, this);
                LosParams.AddIgnoredActor(HitActor);

                const FVector TargetCheckLoc = HitActor->GetActorLocation() + FVector(0.f, 0.f, 30.f);
                if (!World->LineTraceTestByChannel(Start, TargetCheckLoc, ECC_Visibility, LosParams))
                {
                    ValidTargetHit = HitActor;
                    ImpactVisualPoint = Hit.ImpactPoint;
                    break;
                }
            }
        }
    }

#if ENABLE_DRAW_DEBUG
    const bool bDamagedPlayer = (ValidTargetHit != nullptr);
    const FColor TraceColor = bDamagedPlayer ? FColor::Green : FColor::Red;
    const float LifeTime = 1.5f;

    DrawDebugSphere(World, Start, SphereRadius, 12, FColor::Yellow, false, LifeTime, 0, 1.5f);
    DrawDebugSphere(World, End, SphereRadius, 12, TraceColor, false, LifeTime, 0, 1.5f);
    DrawDebugLine(World, Start, End, TraceColor, false, LifeTime, 0, 2.0f);

    if (bDamagedPlayer)
    {
        DrawDebugPoint(World, ImpactVisualPoint, 15.f, FColor::Cyan, false, LifeTime);
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
        CachedAIController->ClearFocus(EAIFocusPriority::Gameplay);
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

void AZombie::RelocateTo(const FVector& NewLocation, const FRotator& NewRotation)
{
    if (!HasAuthority() || bIsDead || PoolState != EZombiePoolState::Alive) return;

    bIsAttacking = false;
    NextRepathAllowedTime = 0.f;
    NextForceRepathTime = 0.f;
    LastTargetLocation = FVector::ZeroVector;

    const float Now = GetWorld()->GetTimeSeconds();
    LastSpawnOrRelocateTime = Now;
    NextRecycleCheckTime = Now + 5.0f;

    if (CachedAIController)
    {
        CachedAIController->StopMovement();
        CachedAIController->ClearFocus(EAIFocusPriority::Gameplay);
    }

    if (CachedMovement)
    {
        CachedMovement->StopMovementImmediately();
        CachedMovement->Velocity = FVector::ZeroVector;
        CachedMovement->bOrientRotationToMovement = true;
        CachedMovement->bUseControllerDesiredRotation = false;
    }

    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
        {
            AnimInst->StopAllMontages(0.f);
        }
        if (IsRunningDedicatedServer())
        {
            MeshComp->bNoSkeletonUpdate = true;
            MeshComp->SetComponentTickEnabled(false);
        }
    }

    const FVector2D RandCircle = FMath::RandPointInCircle(45.f);
    FVector AdjustedSpawn = NewLocation + FVector(RandCircle.X, RandCircle.Y, 0.f);
    const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;

    FHitResult HitFloor;
    const FVector TraceStart = AdjustedSpawn + FVector(0.f, 0.f, 60.f);
    const FVector TraceEnd = AdjustedSpawn - FVector(0.f, 0.f, 300.f);
    FCollisionQueryParams FloorParams(SCENE_QUERY_STAT(ZombieRelocateSnap), false, this);

    if (GetWorld()->LineTraceSingleByChannel(HitFloor, TraceStart, TraceEnd, ECC_WorldStatic, FloorParams))
    {
        AdjustedSpawn = HitFloor.ImpactPoint + FVector(0.f, 0.f, HalfHeight + 2.f);
    }

    FVector FinalLocation = AdjustedSpawn;
    GetWorld()->FindTeleportSpot(this, FinalLocation, NewRotation);

    SetActorLocationAndRotation(FinalLocation, NewRotation, false, nullptr, ETeleportType::TeleportPhysics);

    if (CachedMovement)
    {
        CachedMovement->bJustTeleported = true;
        CachedMovement->SetMovementMode(MOVE_Walking);
    }

    ForceNetUpdate();
}