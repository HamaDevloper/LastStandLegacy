#include "HamaAbilityComponent.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Hama.h"
#include "LastStandLegacyGameState.h"
#include "HamaPlayerState.h"
#include "Components/CapsuleComponent.h"
#include "ZombieDirectorSubsystem.h"

UHamaAbilityComponent::UHamaAbilityComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UHamaAbilityComponent::BeginPlay()
{
    Super::BeginPlay();

    // ⚡ [FIX]: ئەگەر لە BeginPlay دا PlayerState ئامادە نەبوو، Server/Client لە OnRep یان Controller هەوڵ دەدەنەوە
    if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
    {
        if (AHamaPlayerState* PS = OwnerPawn->GetPlayerState<AHamaPlayerState>())
        {
            SetAssignedAbility(PS->GetAssignedRole());
        }
    }
}

void UHamaAbilityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Params;
    Params.bIsPushBased = true;

    Params.Condition = COND_OwnerOnly;
    DOREPLIFETIME_WITH_PARAMS_FAST(UHamaAbilityComponent, CurrentPower, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(UHamaAbilityComponent, bIsAbilityActive, Params);

    Params.Condition = COND_None;
    DOREPLIFETIME_WITH_PARAMS_FAST(UHamaAbilityComponent, bIsGhost, Params);
}

void UHamaAbilityComponent::SetAssignedAbility(EHamaAbilityType NewAbility)
{
    CurrentAssignedAbility = NewAbility;
}

void UHamaAbilityComponent::AddPower(float Amount)
{
    if (bIsAbilityActive || CurrentPower >= MaxPower) return;
    if (!GetOwner() || !GetOwner()->HasAuthority() || CurrentAssignedAbility == EHamaAbilityType::None) return;

    CurrentPower = FMath::Clamp(CurrentPower + Amount, 0.f, MaxPower);
    MARK_PROPERTY_DIRTY_FROM_NAME(UHamaAbilityComponent, CurrentPower, this);

    if (GetOwner()->GetLocalRole() == ROLE_Authority && GetNetMode() != NM_DedicatedServer)
    {
        OnRep_CurrentPower();
    }
}

void UHamaAbilityComponent::StartAbilityCooldown(float Duration)
{
    bIsAbilityActive = true;
    MARK_PROPERTY_DIRTY_FROM_NAME(UHamaAbilityComponent, bIsAbilityActive, this);

    ResetPower();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            AbilityDurationTimerHandle,
            this,
            &UHamaAbilityComponent::EndAbilityCooldown,
            Duration,
            false
        );
    }

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        OnRep_IsAbilityActive();
    }
}

void UHamaAbilityComponent::EndAbilityCooldown()
{
    bIsAbilityActive = false;
    MARK_PROPERTY_DIRTY_FROM_NAME(UHamaAbilityComponent, bIsAbilityActive, this);

    // ⚡ کاتێک Cooldown تەواو بوو، ئەگەر Ghost Mode چالاک بوو دەبێت بکوژێنرێتەوە
    if (bIsGhost)
    {
        DeactivateGhostMode();
    }

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        OnRep_IsAbilityActive();
    }
}

void UHamaAbilityComponent::OnRep_IsAbilityActive()
{
    // Broadcast لێرەدا دەکرێت بۆ UI
}

void UHamaAbilityComponent::ResetPower()
{
    CurrentPower = 0.f;
    MARK_PROPERTY_DIRTY_FROM_NAME(UHamaAbilityComponent, CurrentPower, this);

    if (GetOwner()->GetLocalRole() == ROLE_Authority && GetNetMode() != NM_DedicatedServer)
    {
        OnRep_CurrentPower();
    }
}

bool UHamaAbilityComponent::IsPowerFull() const
{
    return CurrentPower >= MaxPower;
}

void UHamaAbilityComponent::FullPower()
{
    CurrentPower = MaxPower;
    MARK_PROPERTY_DIRTY_FROM_NAME(UHamaAbilityComponent, CurrentPower, this);

    if (GetOwner()->GetLocalRole() == ROLE_Authority && GetNetMode() != NM_DedicatedServer)
    {
        OnRep_CurrentPower();
    }
}

void UHamaAbilityComponent::OnRep_CurrentPower()
{
    OnPowerChanged.Broadcast(CurrentPower);
}

void UHamaAbilityComponent::Server_ActivateAbility_Implementation()
{
    if (CurrentPower < MaxPower || bIsAbilityActive || CurrentAssignedAbility == EHamaAbilityType::None) return;
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    switch (CurrentAssignedAbility)
    {
    case EHamaAbilityType::BulletStorm:
        ActivateBulletStorm();
        break;
    case EHamaAbilityType::MedicalSupport:
        ActivateMedicalSupport();
        break;
    case EHamaAbilityType::GhostMode:
        ActivateGhostMode();
        break;
    case EHamaAbilityType::Blitz:
        ActivateBlitz();
        break;
    default:
        UE_LOG(LogTemp, Warning, TEXT("Player tried to activate ability but has NONE assigned!"));
        break;
    }
}

void UHamaAbilityComponent::ActivateBulletStorm()
{
    UWorld* World = GetWorld();
    if (!World) return;

    if (ALastStandLegacyGameState* GS = World->GetGameState<ALastStandLegacyGameState>())
    {
        GS->StartGlobalBulletStorm(AbilityDuration);
        StartAbilityCooldown(AbilityDuration);
    }
}

void UHamaAbilityComponent::ActivateMedicalSupport()
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority()) return;

    UWorld* World = GetWorld();
    if (!World) return;

    ALastStandLegacyGameState* GS = World->GetGameState<ALastStandLegacyGameState>();
    if (!GS) return;

    const FVector CenterLocation = Owner->GetActorLocation();
    const float SphereRadiusSq = FMath::Square(SphereRadius);
    bool bSuccessfullyRevivedSomeone = false;

    for (APlayerState* PS : GS->PlayerArray)
    {
        if (!PS) continue;

        AHama* TargetHama = Cast<AHama>(PS->GetPawn());
        if (!TargetHama || TargetHama == Owner) continue;

        UHealthComponent* TargetHealth = TargetHama->HealthComponent;
        if (!TargetHealth) continue;

        if (FVector::DistSquared(CenterLocation, TargetHama->GetActorLocation()) <= SphereRadiusSq)
        {
            if (TargetHealth->IsDowned())
            {
                TargetHealth->Revive();
                bSuccessfullyRevivedSomeone = true;
            }
        }
    }

    if (bSuccessfullyRevivedSomeone)
    {
        ResetPower();
    }
}

void UHamaAbilityComponent::ActivateGhostMode()
{
    if (bIsGhost) return;

    bIsGhost = true;
    MARK_PROPERTY_DIRTY_FROM_NAME(UHamaAbilityComponent, bIsGhost, this);

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        if (UWorld* World = GetWorld())
        {
            if (UZombieDirectorSubsystem* Director = World->GetSubsystem<UZombieDirectorSubsystem>())
            {
                if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
                {
                    Director->SetPlayerTargetable(OwnerPawn, false);
                }
            }
        }
    }

    StartAbilityCooldown(AbilityDuration);
    OnRep_IsGhost();
}

void UHamaAbilityComponent::DeactivateGhostMode()
{
    if (!bIsGhost) return;

    bIsGhost = false;
    MARK_PROPERTY_DIRTY_FROM_NAME(UHamaAbilityComponent, bIsGhost, this);

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        if (UWorld* World = GetWorld())
        {
            if (UZombieDirectorSubsystem* Director = World->GetSubsystem<UZombieDirectorSubsystem>())
            {
                if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
                {
                    Director->SetPlayerTargetable(OwnerPawn, true);
                }
            }
        }
    }

    OnRep_IsGhost();
}

void UHamaAbilityComponent::OnRep_IsGhost()
{
    AHama* OwnerChar = Cast<AHama>(GetOwner());
    if (!OwnerChar || !OwnerChar->GetCapsuleComponent()) return;

    if (bIsGhost)
    {
        OwnerChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    }
    else
    {
        OwnerChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    }
}

void UHamaAbilityComponent::ActivateBlitz()
{
    UWorld* World = GetWorld();
    if (!World) return;

    if (ALastStandLegacyGameState* GS = World->GetGameState<ALastStandLegacyGameState>())
    {
        GS->StartTeamAdrenaline(BlitzAbilityDuration);
        StartAbilityCooldown(BlitzAbilityDuration);
    }
}

void UHamaAbilityComponent::StopAllAbilities()
{
    UWorld* World = GetWorld();
    if (World)
    {
        World->GetTimerManager().ClearTimer(AbilityDurationTimerHandle);
    }

    bIsAbilityActive = false;
    MARK_PROPERTY_DIRTY_FROM_NAME(UHamaAbilityComponent, bIsAbilityActive, this);

    if (bIsGhost)
    {
        DeactivateGhostMode();
    }
}