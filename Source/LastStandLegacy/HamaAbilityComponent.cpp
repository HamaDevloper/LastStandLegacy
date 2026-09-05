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
    CachedOwner = Cast<AHama>(GetOwner());
}

ALastStandLegacyGameState* UHamaAbilityComponent::GetGameState() const
{
    if (!CachedGameState && GetWorld())
    {
        CachedGameState = GetWorld()->GetGameState<ALastStandLegacyGameState>();
    }
    return CachedGameState;
}

UZombieDirectorSubsystem* UHamaAbilityComponent::GetZombieDirector() const
{
    if (!CachedDirector && GetWorld())
    {
        CachedDirector = GetWorld()->GetSubsystem<UZombieDirectorSubsystem>();
    }
    return CachedDirector;
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

bool UHamaAbilityComponent::IsAbilityActive() const
{
    return bIsAbilityActive;
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

    if (GetOwner() && GetOwner()->HasAuthority() && GetNetMode() != NM_DedicatedServer)
    {
        OnRep_IsAbilityActive();
    }
}

void UHamaAbilityComponent::EndAbilityCooldown()
{
    bIsAbilityActive = false;
    MARK_PROPERTY_DIRTY_FROM_NAME(UHamaAbilityComponent, bIsAbilityActive, this);

    if (bIsGhost)
    {
        DeactivateGhostMode();
    }

    if (GetOwner() && GetOwner()->HasAuthority() && GetNetMode() != NM_DedicatedServer)
    {
        OnRep_IsAbilityActive();
    }
}

void UHamaAbilityComponent::OnRep_IsAbilityActive()
{
    // Broadcast بۆ UI
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
    if (!CachedOwner || CachedOwner->IsDrinkingPerk()) return;
    if (CachedOwner->IsDowned()) return;

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
    if (ALastStandLegacyGameState* GS = GetGameState())
    {
        GS->StartGlobalBulletStorm(AbilityDuration);
        StartAbilityCooldown(AbilityDuration);
    }
}

bool UHamaAbilityComponent::CanActivateMedicalSupportLocal() const
{
    ALastStandLegacyGameState* GS = GetGameState();
    if (!GS || !CachedOwner) return false;

    const FVector CenterLocation = CachedOwner->GetActorLocation();
    const float SphereRadiusSq = FMath::Square(SphereRadius);

    for (APlayerState* PS : GS->PlayerArray)
    {
        if (!PS) continue;

        AHama* TargetHama = Cast<AHama>(PS->GetPawn());
        if (!TargetHama || TargetHama == CachedOwner) continue;

        if (TargetHama->HealthComponent && TargetHama->HealthComponent->IsDowned())
        {
            if (FVector::DistSquared(CenterLocation, TargetHama->GetActorLocation()) <= SphereRadiusSq)
            {
                return true;
            }
        }
    }

    return false;
}


void UHamaAbilityComponent::ActivateMedicalSupport()
{
    ALastStandLegacyGameState* GS = GetGameState();
    if (!CachedOwner || !CachedOwner->HasAuthority() || !GS) return;

    const FVector CenterLocation = CachedOwner->GetActorLocation();
    const float SphereRadiusSq = FMath::Square(SphereRadius);
    bool bRevivedAnyPlayer = false;

    for (APlayerState* PS : GS->PlayerArray)
    {
        if (!PS) continue;

        AHama* TargetHama = Cast<AHama>(PS->GetPawn());
        if (!TargetHama || TargetHama == CachedOwner) continue;

        UHealthComponent* TargetHealth = TargetHama->HealthComponent;
        if (TargetHealth && TargetHealth->IsDowned())
        {
            if (FVector::DistSquared(CenterLocation, TargetHama->GetActorLocation()) <= SphereRadiusSq)
            {
                TargetHealth->Revive();
                bRevivedAnyPlayer = true;
            }
        }
    }
}

void UHamaAbilityComponent::ActivateGhostMode()
{
    if (bIsGhost) return;

    bIsGhost = true;
    MARK_PROPERTY_DIRTY_FROM_NAME(UHamaAbilityComponent, bIsGhost, this);

    if (UZombieDirectorSubsystem* Director = GetZombieDirector())
    {
        if (CachedOwner && CachedOwner->HasAuthority())
        {
            Director->SetPlayerTargetable(CachedOwner, false);
        }
    }

    StartAbilityCooldown(AbilityDuration);

    if (GetNetMode() != NM_DedicatedServer)
    {
        OnRep_IsGhost();
    }
}

void UHamaAbilityComponent::DeactivateGhostMode()
{
    if (!bIsGhost) return;

    bIsGhost = false;
    MARK_PROPERTY_DIRTY_FROM_NAME(UHamaAbilityComponent, bIsGhost, this);

    if (UZombieDirectorSubsystem* Director = GetZombieDirector())
    {
        if (CachedOwner && CachedOwner->HasAuthority())
        {
            Director->SetPlayerTargetable(CachedOwner, true);
        }
    }

    if (GetNetMode() != NM_DedicatedServer)
    {
        OnRep_IsGhost();
    }
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
    if (ALastStandLegacyGameState* GS = GetGameState())
    {
        GS->StartTeamAdrenaline(BlitzAbilityDuration);
        StartAbilityCooldown(BlitzAbilityDuration);
    }
}

void UHamaAbilityComponent::StopAllAbilities()
{
    if (UWorld* World = GetWorld())
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