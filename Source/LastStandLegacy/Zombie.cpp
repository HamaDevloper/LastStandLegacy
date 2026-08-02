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

AZombie::AZombie()
{
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;

    bReplicates = true;
    SetReplicateMovement(true);

    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    SetNetUpdateFrequency(10.f);
    SetMinNetUpdateFrequency(2.f);

    MaxHealth = BaseHealth;
    Health = MaxHealth;

    AttackDistanceSq = FMath::Square(AttackDistance);
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
    }

    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        if (!IsRunningDedicatedServer())
        {
            MeshComp->bEnableUpdateRateOptimizations = true;
            MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
            MeshComp->bCastFarShadow = false;
            MeshComp->SetGenerateOverlapEvents(false);
        }
        else
        {
            MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
            MeshComp->bNoSkeletonUpdate = true;
        }
    }

    if (!HasAuthority()) return;

    if (!MeshToSelect.IsEmpty())
    {
        MeshIndexSelected = static_cast<uint8>(FMath::RandRange(0, MeshToSelect.Num() - 1));
        ApplySelectedMesh();
    }

    CachedGS = GetWorld()->GetGameState<ALastStandLegacyGameState>();
    CachedDirector = GetWorld()->GetSubsystem<UZombieDirectorSubsystem>();

    if (CachedDirector)
    {
        CachedDirector->RegisterZombie(this);
        bRegisteredWithDirector = true;
    }
}

void AZombie::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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
  
    DOREPLIFETIME_CONDITION(AZombie, MeshIndexSelected, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(AZombie, MaxHealth, COND_InitialOnly);
}

void AZombie::SetStatsForRound(int32 CurrentRound)
{
    if (!HasAuthority())
    {
        return;
    }

    const float NewHealth = BaseHealth * FMath::Pow(1.12f, CurrentRound - 1);

    MaxHealth = FMath::Clamp(NewHealth, BaseHealth, MaxHealthZombieReach);
    Health = MaxHealth;

    const float Alpha = FMath::Clamp((CurrentRound - 1) / 19.f, 0.f, 1.f);
    const float BaseSpeed = FMath::Lerp(MinWalkSpeed, MaxBaseWalkSpeed, Alpha);

    constexpr float TierOffsets[] =
    {
        -40.f,
        -20.f,
         0.f,
         30.f,
         70.f
    };

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

    Streamable.RequestAsyncLoad(
        SoftMesh.ToSoftObjectPath(),
        [this, SoftMesh]()
        {
            if (USkeletalMesh* AsyncLoadedMesh = SoftMesh.Get())
            {
                if (USkeletalMeshComponent* MeshComp = GetMesh())
                {
                    MeshComp->SetSkeletalMeshAsset(AsyncLoadedMesh);
                }
            }
        }
    );
}

void AZombie::ExecuteMeleeHit()
{
    if (!HasAuthority() || bIsDead || !CurrentTarget)
    {
        return;
    }

    const float DistSq =
        FVector::DistSquared(GetActorLocation(), CurrentTarget->GetActorLocation());

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

float AZombie::TakeDamage(
    float DamageAmount,
    FDamageEvent const& DamageEvent,
    AController* EventInstigator,
    AActor* DamageCauser)
{
    if (!HasAuthority() || bIsDead)
    {
        return 0.f;
    }

    float DamageApplied =
        Super::TakeDamage(
            DamageAmount,
            DamageEvent,
            EventInstigator,
            DamageCauser);

    const bool bIsMelee =
        DamageEvent.DamageTypeClass &&
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

    Health -= DamageApplied;

    AHamaPlayerState* TargetPlayerState =
        EventInstigator
        ? EventInstigator->GetPlayerState<AHamaPlayerState>()
        : nullptr;

    if (Health <= 0.f)
    {
        if (TargetPlayerState)
        {
            const int32 Points = bIsMelee ? 130 : 100;

            TargetPlayerState->AddPoints(
                bDoublePoints ? Points * 2 : Points);

            TargetPlayerState->AddKills(1);
        }

        Die(EventInstigator);
    }
    else if (TargetPlayerState)
    {
        const int32 Points = bIsMelee ? 20 : 10;

        TargetPlayerState->AddPoints(
            bDoublePoints ? Points * 2 : Points);
    }

    return DamageApplied;
}

void AZombie::Die(AController* KillerController)
{
    if (bIsDead || !HasAuthority())
    {
        return;
    }

    bIsDead = true;
    MARK_PROPERTY_DIRTY_FROM_NAME(AZombie, bIsDead, this);

    SetNetUpdateFrequency(1.f);

    if (bRegisteredWithDirector && CachedDirector)
    {
        CachedDirector->UnregisterZombie(this);
        bRegisteredWithDirector = false;
    }

    if (CachedAIController)
    {
        CachedAIController->StopMovement();
        CachedAIController->UnPossess();
    }

    OnRep_IsDead();

    OnZombieDeath.ExecuteIfBound(this, KillerController);
}

void AZombie::OnRep_IsDead()
{
    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
    }

    if (USkeletalMeshComponent* CharacterMesh = GetMesh())
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

    SetLifeSpan(2.f);
}