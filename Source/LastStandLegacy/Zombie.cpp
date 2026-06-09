#include "Zombie.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Hama.h" 
#include "LastStandLegacyGameMode.h" // بۆ پەیوەندی بە GameMode
#include "Net/UnrealNetwork.h"

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
        GetWorld()->GetTimerManager().SetTimer(ChaseTimerHandle, this, &AZombie::UpdateNearestTarget, 1.0f, true);
        GetWorld()->GetTimerManager().SetTimer(AttackTimerHandle, this, &AZombie::CheckAttackRange, 0.5f, true);
    }
}

void AZombie::SetStatsForRound(int32 CurrentRound)
{
    if (!HasAuthority()) return;

    // ── خوێن ──────────────────────────────────────────
    float NewHealth = BaseHealth * FMath::Pow(1.12f, CurrentRound - 1);
    MaxHealth = FMath::Clamp(NewHealth, BaseHealth, 60000.f);
    Health = MaxHealth;

    // ── خێرایی ────────────────────────────────────────
    // بنەمای خێرایی بەپێی ڕاوند (Lerp)
    float Alpha = FMath::Clamp((CurrentRound - 1) / 49.f, 0.f, 1.f);
    float BaseSpeed = FMath::Lerp(180.f, 550.f, Alpha);

    // Tier جیاوازی زیاد دەکات
    // هەرچی ڕاوند زیاتر → Tier زیاتر بەردەست دەبن
    const float TierOffsets[] = { -30.f, 0.f, 0.f, +30.f, +70.f };
    int32 MaxTier = FMath::Clamp(CurrentRound, 1, 5);
    int32 RandTier = FMath::RandRange(0, MaxTier - 1);
    float TierBonus = TierOffsets[RandTier];

    float FinalSpeed = FMath::Clamp(BaseSpeed + TierBonus, 180.f, 550.f);
    GetCharacterMovement()->MaxWalkSpeed = FinalSpeed;
}

void AZombie::UpdateNearestTarget()
{
    if (bIsDead) return;

    AAIController* AICont = Cast<AAIController>(GetController());
    if (!AICont) return;

    APawn* NearestPlayer = nullptr;
    float ClosestDistanceSq = UE_BIG_NUMBER;
    FVector ZombieLocation = GetActorLocation();

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (APawn* PlayerPawn = It->Get()->GetPawn())
        {
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
        AICont->MoveToActor(CurrentTarget, 40.f, true, true, true);
    }
}

void AZombie::CheckAttackRange()
{
    if (!CurrentTarget || bIsDead) return;

    float DistSq = FVector::DistSquared(GetActorLocation(), CurrentTarget->GetActorLocation());
    if (DistSq < FMath::Square(AttackDistance))
    {
        UGameplayStatics::ApplyDamage(CurrentTarget, AttackDamage, GetController(), this, UDamageType::StaticClass());
    }
}

float AZombie::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (!HasAuthority() || bIsDead) return 0.f;

    float DamageApplied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    Health -= DamageApplied;

    if (Health <= 0.f)
    {
        Die(EventInstigator);
    }
    else if (AHama* Attacker = Cast<AHama>(EventInstigator ? EventInstigator->GetPawn() : nullptr))
    {
        Attacker->Points += 10;
    }

    return DamageApplied;
}

void AZombie::Die(AController* KillerController)
{
    if (bIsDead || !HasAuthority()) return;
    bIsDead = true;

    GetWorld()->GetTimerManager().ClearTimer(ChaseTimerHandle);
    GetWorld()->GetTimerManager().ClearTimer(AttackTimerHandle);

    if (AAIController* AICont = Cast<AAIController>(GetController()))
    {
        AICont->StopMovement();
        AICont->UnPossess();
    }

    if (AHama* KillerChar = Cast<AHama>(KillerController ? KillerController->GetPawn() : nullptr))
    {
        KillerChar->Points += KillPointsValue;
    }

    // ئاگادارکردنەوەی GameMode بۆ هەژمارکردنی مردنەکە
    if (ALastStandLegacyGameMode* GM = Cast<ALastStandLegacyGameMode>(GetWorld()->GetAuthGameMode()))
    {
        GM->ZombieDied();
    }
    OnRep_IsDead();
}

void AZombie::OnRep_IsDead()
{
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    GetMesh()->SetSimulatePhysics(true);
    GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));

    if (UCapsuleComponent* Caps = GetCapsuleComponent())
    {
        Caps->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    SetLifeSpan(8.0f);
}

void AZombie::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AZombie, bIsDead);
}