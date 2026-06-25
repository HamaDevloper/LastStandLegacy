#include "Zombie.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Hama.h"
#include "HamaAbilityComponent.h"
#include "LastStandLegacyGameMode.h"
#include "LastStandLegacyGameState.h"
#include "Net/UnrealNetwork.h"
#include "HamaPlayerState.h"

AZombie::AZombie()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    SetNetUpdateFrequency(10.f);
    SetMinNetUpdateFrequency(3.f);

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

    if (!HasAuthority()) return;
    CachedGS = GetWorld()->GetGameState<ALastStandLegacyGameState>();

    const float RandomChaseDelay = FMath::RandRange(0.1f, 1.0f);
    const float RandomAttackDelay = FMath::RandRange(0.1f, 0.5f);

    GetWorld()->GetTimerManager().SetTimer(
        ChaseTimerHandle, this, &AZombie::UpdateNearestTarget, 1.0f, true, RandomChaseDelay);

    GetWorld()->GetTimerManager().SetTimer(
        AttackTimerHandle, this, &AZombie::CheckAttackRange, 0.5f, true, RandomAttackDelay);
}

void AZombie::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AZombie, bIsDead);
    DOREPLIFETIME(AZombie, Health);
    DOREPLIFETIME_CONDITION(AZombie, MaxHealth, COND_InitialOnly);
}

void AZombie::SetStatsForRound(int32 CurrentRound)
{
    if (!HasAuthority()) return;

    const float NewHealth = BaseHealth * FMath::Pow(1.12f, CurrentRound - 1);
    MaxHealth = FMath::Clamp(NewHealth, BaseHealth, 60000.f);
    Health = MaxHealth;

    const float Alpha = FMath::Clamp((CurrentRound - 1) / 19.f, 0.f, 1.f);
    const float BaseSpeed = FMath::Lerp(200.f, 550.f, Alpha);

    const float TierOffsets[] = { -30.f, 0.f, 0.f, +30.f, +70.f };
    const int32 MaxTier = FMath::Clamp(CurrentRound, 1, 5);
    const int32 RandTier = FMath::RandRange(0, MaxTier - 1);

    GetCharacterMovement()->MaxWalkSpeed =
        FMath::Clamp(BaseSpeed + TierOffsets[RandTier], 200.f, 550.f);
}

void AZombie::UpdateNearestTarget()
{
    if (bIsDead || !CachedAIController || !CachedGS) return;

    APawn* NearestPlayer = nullptr;
    float  ClosestDistanceSq = UE_BIG_NUMBER;
    const FVector ZombieLocation = GetActorLocation();

    for (APawn* Candidate : CachedGS->ValidTargets)
    {
        if (!IsValid(Candidate)) continue;

        const float DistSq = FVector::DistSquared(ZombieLocation, Candidate->GetActorLocation());
        if (DistSq < ClosestDistanceSq)
        {
            ClosestDistanceSq = DistSq;
            NearestPlayer = Candidate;
        }
    }

    if (NearestPlayer && NearestPlayer != CurrentTarget)
    {
        CurrentTarget = NearestPlayer;
        CachedAIController->MoveToActor(CurrentTarget, 40.f, true, true, true);
    }
    else if (!NearestPlayer && CurrentTarget)
    {
        CurrentTarget = nullptr;
        CachedAIController->StopMovement();
    }

    // ── گۆڕینی ڕێژەی نوێکردنەوە بەپێی مەسافە ──
    if (CurrentTarget)
    {
        const float DistSq = FVector::DistSquared(ZombieLocation, CurrentTarget->GetActorLocation());
        const float NewRate = (DistSq > FMath::Square(2000.f)) ? 2.0f : 1.0f;
        GetWorld()->GetTimerManager().SetTimer(
            ChaseTimerHandle, this, &AZombie::UpdateNearestTarget, NewRate, false);
    }
}

void AZombie::CheckAttackRange()
{
    if (bIsDead || !CurrentTarget) return;

    if (FVector::DistSquared(GetActorLocation(), CurrentTarget->GetActorLocation()) < AttackDistanceSq)
    {
        UGameplayStatics::ApplyDamage(
            CurrentTarget, AttackDamage, GetController(), this, UDamageType::StaticClass());
    }
}

float AZombie::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
    AController* EventInstigator, AActor* DamageCauser)
{
    if (!HasAuthority() || bIsDead) return 0.f;

    float DamageApplied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    // ── CachedGS بەکاردێت لێرەشدا ──
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

    AHamaPlayerState* AttackerPS =
        EventInstigator ? EventInstigator->GetPlayerState<AHamaPlayerState>() : nullptr;

    if (Health <= 0.f)
    {
        if (AttackerPS)
        {
            AttackerPS->AddPoints(bDoublePoints ? 200 : 100);
            AttackerPS->AddKills(1);
        }
        Die(EventInstigator);
    }
    else
    {
        if (AttackerPS)
        {
            AttackerPS->AddPoints(bDoublePoints ? 20 : 10);
        }
    }

    return DamageApplied;
}

void AZombie::Die(AController* KillerController)
{
    if (bIsDead || !HasAuthority()) return;

    bIsDead = true;

    SetNetUpdateFrequency(2.f);

    GetWorld()->GetTimerManager().ClearTimer(ChaseTimerHandle);
    GetWorld()->GetTimerManager().ClearTimer(AttackTimerHandle);

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
    USkeletalMeshComponent* SkeletalMesh = GetMesh();
    if (!SkeletalMesh) return;

    SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    SkeletalMesh->SetSimulatePhysics(true);
    SkeletalMesh->SetCollisionProfileName(TEXT("Ragdoll"));

    if (UCapsuleComponent* Caps = GetCapsuleComponent())
    {
        Caps->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    SetLifeSpan(2.0f);
}