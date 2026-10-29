#include "HamaAbilityComponent.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Hama.h"
#include "LastStandLegacyGameState.h"
#include "HamaPlayerState.h"
#include "HealthComponent.h"
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

    FDoRepLifetimeParams OwnerParams;
    OwnerParams.bIsPushBased = true;
    OwnerParams.Condition = COND_OwnerOnly;

    DOREPLIFETIME_WITH_PARAMS_FAST(UHamaAbilityComponent, CurrentPower, OwnerParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(UHamaAbilityComponent, bIsAbilityActive, OwnerParams);

    FDoRepLifetimeParams SharedParams;
    SharedParams.bIsPushBased = true;
    SharedParams.Condition = COND_None;

    DOREPLIFETIME_WITH_PARAMS_FAST(UHamaAbilityComponent, bIsGhost, SharedParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(UHamaAbilityComponent, bIsBrawler, SharedParams);
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

    if (GetOwner()->HasAuthority() && GetNetMode() != NM_DedicatedServer)
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

    if (bIsBrawler)
    {
        DeactivateBrawler();
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

    if (GetOwner() && GetOwner()->HasAuthority() && GetNetMode() != NM_DedicatedServer)
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

    if (GetOwner() && GetOwner()->HasAuthority() && GetNetMode() != NM_DedicatedServer)
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
    if (!CachedOwner) CachedOwner = Cast<AHama>(GetOwner());
    if (!CachedOwner || CachedOwner->IsDrinkingPerk() || CachedOwner->IsDowned()) return;

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
    case EHamaAbilityType::Brawler:
        ActivateBrawler();
        break;
    default:
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

void UHamaAbilityComponent::ActivateMedicalSupport()
{
    ALastStandLegacyGameState* GS = GetGameState();
    if (!CachedOwner || !CachedOwner->HasAuthority() || !GS) return;

    const FVector CenterLocation = CachedOwner->GetActorLocation();
    const float SphereRadiusSq = FMath::Square(SphereRadius);

    for (APlayerState* PS : GS->PlayerArray)
    {
        if (!PS) continue;
        APawn* TargetPawn = PS->GetPawn();
        if (!TargetPawn || TargetPawn == CachedOwner) continue;

        if (UHealthComponent* TargetHealth = TargetPawn->FindComponentByClass<UHealthComponent>())
        {
            if (TargetHealth->IsDowned() && FVector::DistSquared(CenterLocation, TargetPawn->GetActorLocation()) <= SphereRadiusSq)
            {
                TargetHealth->Revive();
            }
        }
    }

    // سفرکردنەوەی پاوەر دوای زیندووکردنەوەی تیمەکە
    ResetPower();
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
    if (!CachedOwner) CachedOwner = Cast<AHama>(GetOwner());
    if (!CachedOwner || !CachedOwner->GetCapsuleComponent()) return;

    if (bIsGhost)
    {
        CachedOwner->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    }
    else
    {
        CachedOwner->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    }
}

void UHamaAbilityComponent::ActivateBrawler()
{
    if (bIsBrawler) return;

    bIsBrawler = true;
    MARK_PROPERTY_DIRTY_FROM_NAME(UHamaAbilityComponent, bIsBrawler, this);

    StartAbilityCooldown(BrawlerAbilityDuration);

    if (GetNetMode() != NM_DedicatedServer)
    {
        OnRep_IsBrawler();
    }
}

void UHamaAbilityComponent::DeactivateBrawler()
{
    if (!bIsBrawler) return;

    bIsBrawler = false;
    MARK_PROPERTY_DIRTY_FROM_NAME(UHamaAbilityComponent, bIsBrawler, this);

    if (GetNetMode() != NM_DedicatedServer)
    {
        OnRep_IsBrawler();
    }
}

void UHamaAbilityComponent::OnRep_IsBrawler()
{
    if (!CachedOwner) CachedOwner = Cast<AHama>(GetOwner());
    if (CachedOwner)
    {
        CachedOwner->OnBrawlerStateChanged(bIsBrawler);
    }
}

void UHamaAbilityComponent::StopAllAbilities()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(AbilityDurationTimerHandle);
    }

    if (bIsAbilityActive)
    {
        bIsAbilityActive = false;
        MARK_PROPERTY_DIRTY_FROM_NAME(UHamaAbilityComponent, bIsAbilityActive, this);

        if (GetNetMode() != NM_DedicatedServer)
        {
            OnRep_IsAbilityActive();
        }
    }

    if (bIsGhost)
    {
        DeactivateGhostMode();
    }

    if (bIsBrawler)
    {
        DeactivateBrawler();
    }
}