#include "Zombie.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Hama.h"
#include "HamaAbilityComponent.h"
#include "LastStandLegacyGameMode.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "HamaPlayerState.h"
#include "LastStandLegacyGameState.h"

AZombie::AZombie()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    SetNetUpdateFrequency(10.f);
    SetMinNetUpdateFrequency(3.f);

    MaxHealth = BaseHealth;
    Health = MaxHealth;
}

// ── وەرگرتنی کۆنتڕۆڵەر تەنها یەک جار ──
void AZombie::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    // کۆنتڕۆڵەرەکە کاش دەکەین بۆ ئەوەی چیتر پێویستمان بە Cast نەبێت لەناو تایمەرەکاندا
    CachedAIController = Cast<AAIController>(NewController);
}

void AZombie::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        // ── بڵاوکردنەوەی کاتی تایمەرەکان بۆ ڕێگری لە لاگ ──
        float RandomChaseDelay = FMath::RandRange(0.1f, 1.0f);
        float RandomAttackDelay = FMath::RandRange(0.1f, 0.5f);

        GetWorld()->GetTimerManager().SetTimer(
            ChaseTimerHandle, this, &AZombie::UpdateNearestTarget, 1.0f, true, RandomChaseDelay);

        GetWorld()->GetTimerManager().SetTimer(
            AttackTimerHandle, this, &AZombie::CheckAttackRange, 0.5f, true, RandomAttackDelay);
    }
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

    float NewHealth = BaseHealth * FMath::Pow(1.12f, CurrentRound - 1);
    MaxHealth = FMath::Clamp(NewHealth, BaseHealth, 60000.f);
    Health = MaxHealth;

    float Alpha = FMath::Clamp((CurrentRound - 1) / 19.f, 0.f, 1.f);
    float BaseSpeed = FMath::Lerp(200.f, 550.f, Alpha);

    const float TierOffsets[] = { -30.f, 0.f, 0.f, +30.f, +70.f };
    int32 MaxTier = FMath::Clamp(CurrentRound, 1, 5);
    int32 RandTier = FMath::RandRange(0, MaxTier - 1);
    float TierBonus = TierOffsets[RandTier];

    float FinalSpeed = FMath::Clamp(BaseSpeed + TierBonus, 200.f, 550.f);
    GetCharacterMovement()->MaxWalkSpeed = FinalSpeed;
}

void AZombie::UpdateNearestTarget()
{
    // ئەگەر مردووە یان کۆنتڕۆڵەرەکەی نییە، ڕاستەوخۆ بیوەستێنە
    if (bIsDead || !CachedAIController) return;

    APawn* NearestPlayer = nullptr;
    float ClosestDistanceSq = UE_BIG_NUMBER;
    FVector ZombieLocation = GetActorLocation();

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (!It->IsValid()) continue;

        if (APawn* PlayerPawn = It->Get()->GetPawn())
        {
            // ── ئۆپتیمایزکردن و پشکنینی خێو ──
            if (AHama* HamaPlayer = Cast<AHama>(PlayerPawn))
            {
                if (UHamaAbilityComponent* AbilityComp = HamaPlayer->FindComponentByClass<UHamaAbilityComponent>())
                {
                    if (AbilityComp->bIsGhost)
                    {
                        continue; // پشتگوێخستنی یاریزانی خێو
                    }
                }
            }

            float DistanceSq = FVector::DistSquared(ZombieLocation, PlayerPawn->GetActorLocation());

            if (DistanceSq < ClosestDistanceSq)
            {
                ClosestDistanceSq = DistanceSq;
                NearestPlayer = PlayerPawn;
            }
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
}

void AZombie::CheckAttackRange()
{
    if (!CurrentTarget || bIsDead) return;

    float DistSq = FVector::DistSquared(GetActorLocation(), CurrentTarget->GetActorLocation());

    if (DistSq < FMath::Square(AttackDistance))
    {
        UGameplayStatics::ApplyDamage(
            CurrentTarget, AttackDamage, GetController(), this, UDamageType::StaticClass());
    }
}

float AZombie::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (!HasAuthority() || bIsDead) return 0.f;

    float DamageApplied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>();
    bool bDoublePoints = false;

    // یەکەم جار پشکنین دەکەین بزانین GS هەیە، پاشان زانیاری لێ وەردەگرین
    if (GS)
    {
        bDoublePoints = GS->bIsDoublePointsActive;
        if (GS->bHasInstaKill)
        {
            DamageApplied = Health;
        }
    }
    Health -= DamageApplied;

    AHamaPlayerState* AttackerPS = EventInstigator ? EventInstigator->GetPlayerState<AHamaPlayerState>() : nullptr;

    if (Health <= 0.f)
    {
        if (AttackerPS)
        {
            int32 PointsToGive = bDoublePoints ? 200 : 100;
            AttackerPS->AddPoints(PointsToGive);
            AttackerPS->AddKills(1);
        }
        Die(EventInstigator);
    }
    else
    {
        if (AttackerPS)
        {
            int32 PointsToGive = bDoublePoints ? 20 : 10;
            AttackerPS->AddPoints(PointsToGive);
        }
    }

    return DamageApplied;
}

void AZombie::Die(AController* KillerController)
{
    if (bIsDead || !HasAuthority()) return;

    bIsDead = true;

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