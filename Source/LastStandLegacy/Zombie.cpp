#include "Zombie.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Hama.h"
#include "LastStandLegacyGameMode.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "HamaPlayerState.h"

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

void AZombie::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        GetWorld()->GetTimerManager().SetTimer(
            ChaseTimerHandle, this, &AZombie::UpdateNearestTarget, 1.0f, true);
        GetWorld()->GetTimerManager().SetTimer(
            AttackTimerHandle, this, &AZombie::CheckAttackRange, 0.5f, true);
    }
}

void AZombie::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // bIsDead — هەموو کلاینت پێویستیانە بۆ Ragdoll
    DOREPLIFETIME(AZombie, bIsDead);

    // Health — بۆ HealthBar UI
    DOREPLIFETIME(AZombie, Health);

    // MaxHealth — تەنها جارێک لە سەرەتا
    DOREPLIFETIME_CONDITION(AZombie, MaxHealth, COND_InitialOnly);
}

void AZombie::SetStatsForRound(int32 CurrentRound)
{
    if (!HasAuthority()) return;

    // ── خوێن ──────────────────────────────────────────
    float NewHealth = BaseHealth * FMath::Pow(1.12f, CurrentRound - 1);
    MaxHealth = FMath::Clamp(NewHealth, BaseHealth, 60000.f);
    Health = MaxHealth;

    // ── خێرایی: ──────────────────────────────────────
    float Alpha = FMath::Clamp((CurrentRound - 1) / 19.f, 0.f, 1.f);
    float BaseSpeed = FMath::Lerp(200.f, 550.f, Alpha);

    // زیادکردنی Tier (بۆ ئەوەی هەمیشە هەموویان یەک خێرایی نەبن)
    const float TierOffsets[] = { -30.f, 0.f, 0.f, +30.f, +70.f };
    int32 MaxTier = FMath::Clamp(CurrentRound, 1, 5);
    int32 RandTier = FMath::RandRange(0, MaxTier - 1);
    float TierBonus = TierOffsets[RandTier];

    // خێرایی کۆتایی
    float FinalSpeed = FMath::Clamp(BaseSpeed + TierBonus, 200.f, 550.f);
    GetCharacterMovement()->MaxWalkSpeed = FinalSpeed;
}

void AZombie::UpdateNearestTarget()
{
    if (bIsDead) return;

    AAIController* AICont = Cast<AAIController>(GetController());
    if (!AICont) return;

    APawn* NearestPlayer = nullptr;
    float   ClosestDistanceSq = UE_BIG_NUMBER;
    FVector ZombieLocation = GetActorLocation();

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        // چاک کرا: Iterator چێک کراوە پێش بەکارهێنان
        if (!It->IsValid()) continue;

        if (APawn* PlayerPawn = It->Get()->GetPawn())
        {
            float DistanceSq = FVector::DistSquared(
                ZombieLocation, PlayerPawn->GetActorLocation());

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
        AICont->MoveToActor(CurrentTarget, 40.f, true, true, true);
    }
}

void AZombie::CheckAttackRange()
{
    if (!CurrentTarget || bIsDead) return;

    float DistSq = FVector::DistSquared(
        GetActorLocation(), CurrentTarget->GetActorLocation());

    if (DistSq < FMath::Square(AttackDistance))
    {
        UGameplayStatics::ApplyDamage(
            CurrentTarget, AttackDamage,
            GetController(), this, UDamageType::StaticClass());
    }
}

float AZombie::TakeDamage(
    float DamageAmount,
    FDamageEvent const& DamageEvent,
    AController* EventInstigator,
    AActor* DamageCauser)
{
    if (!HasAuthority() || bIsDead) return 0.f;

    float DamageApplied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    Health -= DamageApplied;

    // ڕاستەوخۆ PlayerState دەهێنین لە کۆنتڕۆڵەرەکەوە، بێ ئەوەی Cast بکەین بۆ AHama
    AHamaPlayerState* AttackerPS = EventInstigator ? EventInstigator->GetPlayerState<AHamaPlayerState>() : nullptr;

    if (Health <= 0.f)
    {
        if (AttackerPS)
        {
            AttackerPS->AddPoints(100);
            AttackerPS->AddKills(1);
        }

        Die(EventInstigator);
    }
    else
    {
        if (AttackerPS)   AttackerPS->AddPoints(10);
    }

    return DamageApplied;
}

void AZombie::Die(AController* KillerController)
{
    if (bIsDead || !HasAuthority()) return;

    bIsDead = true;

    // وەستاندنی تایمەرەکان
    GetWorld()->GetTimerManager().ClearTimer(ChaseTimerHandle);
    GetWorld()->GetTimerManager().ClearTimer(AttackTimerHandle);

    if (AAIController* AICont = Cast<AAIController>(GetController()))
    {
        AICont->StopMovement();
        AICont->UnPossess();
    }

    OnRep_IsDead();

    OnZombieDeath.Broadcast(this);
}

void AZombie::OnRep_IsDead()
{
    // چاک کرا: GetMesh() چێک کراوە پێش بەکارهێنان
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