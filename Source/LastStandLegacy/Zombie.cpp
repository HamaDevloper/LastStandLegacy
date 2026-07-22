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
#include "Net/Core/PushModel/PushModel.h"
#include "HamaPlayerState.h"
#include "Engine/DamageEvents.h"
#include "MeleeDamageType.h"
#include "ZombieDirectorSubsystem.h" 

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

    if (UWorld* World = GetWorld())
    {
        if (UZombieDirectorSubsystem* Director = World->GetSubsystem<UZombieDirectorSubsystem>())
        {
            Director->RegisterZombie(this);
        }
    }
}

void AZombie::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    if (HasAuthority())
    {
        if (UWorld* World = GetWorld())
        {
            if (UZombieDirectorSubsystem* Director = World->GetSubsystem<UZombieDirectorSubsystem>())
            {
                Director->UnregisterZombie(this);
            }
        }
    }
}

void AZombie::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Params;
    Params.bIsPushBased = true;

    DOREPLIFETIME_WITH_PARAMS_FAST(AZombie, bIsDead, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AZombie, Health, Params);
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

    GetCharacterMovement()->MaxWalkSpeed = FMath::Clamp(BaseSpeed + TierOffsets[RandTier], 200.f, 550.f);
}

// ── سیستەمی نوێی دیمیج دان (جێگرەوەی CheckAttackRange) ──
void AZombie::ExecuteMeleeHit()
{
    if (!HasAuthority() || bIsDead || !CurrentTarget) return;

    // پشکنینی سێرڤەر بۆ دڵنیابوونەوە لەوەی یاریزانەکە ڕاینەکردووە (Desync & Anti-Ghost Hit)
    float CurrentDistSq = FVector::DistSquared(GetActorLocation(), CurrentTarget->GetActorLocation());

    // بڕێک یەدەگ (Margin) دادەنێین بۆ جوڵەی خێرای یاریزان (نموونە: 50 یەکە)
    float ToleranceSq = FMath::Square(AttackDistance + 50.f);

    if (CurrentDistSq <= ToleranceSq)
    {
        UGameplayStatics::ApplyDamage(
            CurrentTarget, AttackDamage, GetController(), this, UDamageType::StaticClass());
    }
}

float AZombie::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (!HasAuthority() || bIsDead) return 0.f;

    float DamageApplied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    bool bIsMeleeDamage = DamageEvent.DamageTypeClass && DamageEvent.DamageTypeClass->IsChildOf(UMeleeDamageType::StaticClass());
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
    MARK_PROPERTY_DIRTY_FROM_NAME(AZombie, Health, this);

#if !UE_BUILD_SHIPPING
    GEngine->AddOnScreenDebugMessage(555, 2.f, FColor::Red, FString::Printf(TEXT("Current Zombie Health Is %f"), Health));
#endif

    AHamaPlayerState* AttackerPS = EventInstigator ? EventInstigator->GetPlayerState<AHamaPlayerState>() : nullptr;

    if (Health <= 0.f)
    {
        if (AttackerPS)
        {
            int32 KillPoints = bIsMeleeDamage ? 130 : 100;
            AttackerPS->AddPoints(bDoublePoints ? (KillPoints * 2) : KillPoints);
            AttackerPS->AddKills(1);
        }
        Die(EventInstigator);
    }
    else
    {
        if (AttackerPS)
        {
            int32 HitPoints = bIsMeleeDamage ? 20 : 10;
            AttackerPS->AddPoints(bDoublePoints ? (HitPoints * 2) : HitPoints);
        }
    }

    return DamageApplied;
}

void AZombie::Die(AController* KillerController)
{
    if (bIsDead || !HasAuthority()) return;

    bIsDead = true;
    MARK_PROPERTY_DIRTY_FROM_NAME(AZombie, bIsDead, this);

    SetNetUpdateFrequency(1.f);

    if (UWorld* World = GetWorld())
    {
        if (UZombieDirectorSubsystem* Director = World->GetSubsystem<UZombieDirectorSubsystem>())
        {
            Director->UnregisterZombie(this);
        }
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