#include "HamaAbilityComponent.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Hama.h"
#include "LastStandLegacyGameState.h"
#include "CollisionQueryParams.h"
#include "HamaPlayerState.h"
#include "Engine/OverlapResult.h"
#include "Components/CapsuleComponent.h"
#include "ZombieDirectorSubsystem.h"
#include "DrawDebugHelpers.h"

UHamaAbilityComponent::UHamaAbilityComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UHamaAbilityComponent::BeginPlay()
{
    Super::BeginPlay();

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

    Params.Condition = COND_None;
    DOREPLIFETIME_WITH_PARAMS_FAST(UHamaAbilityComponent, bIsGhost, Params);
}

void UHamaAbilityComponent::SetAssignedAbility(EHamaAbilityType NewAbility)
{
    CurrentAssignedAbility = NewAbility;
}

void UHamaAbilityComponent::AddPower(float Amount)
{
    if (CurrentPower >= MaxPower) return;
    if (!GetOwner()->HasAuthority() || CurrentAssignedAbility == EHamaAbilityType::None) return;

    CurrentPower = FMath::Clamp(CurrentPower + Amount, 0.f, MaxPower);

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Magenta, FString::Printf(TEXT("Current Power is: %f"), CurrentPower));

    if (GetOwner()->GetLocalRole() == ROLE_Authority && GetNetMode() != NM_DedicatedServer)
    {
        OnRep_CurrentPower();
    }
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

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Magenta, FString::Printf(TEXT("Current Power is: %f"), CurrentPower));
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
    if (CurrentPower < MaxPower || CurrentAssignedAbility == EHamaAbilityType::None) return;
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
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("BulletStorm Activated on Server!"));
        ResetPower();
        return;
    }
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("BulletStorm Failed!"));
}

void UHamaAbilityComponent::ActivateMedicalSupport()
{
    UWorld* World = GetWorld();
    AActor* Owner = GetOwner();
    if (!World || !Owner) return;

    FVector CenterLocation = Owner->GetActorLocation();
    TArray<FOverlapResult> OverlapResults; // گۆڕا بۆ OverlapResult
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(SphereRadius);

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Owner); // خۆت فەرامۆش دەکەیت بۆ ئەوەی خۆت نەگریت

    // بەکارهێنانی Overlap لەبری Sweep
    bool bHit = World->OverlapMultiByChannel(OverlapResults, CenterLocation, FQuat::Identity, ECC_Pawn, SphereShape, Params);
    bool bSuccessfullyRevivedSomeone = false;

    if (bHit)
    {
        for (const FOverlapResult& Overlap : OverlapResults)
        {
            // وەرگرتنی ئەکتەرەکە لە Overlapـەوە
            if (AHama* Hama = Cast<AHama>(Overlap.GetActor()))
            {
                if (UHamaComponent* HamaComponent = Hama->FindComponentByClass<UHamaComponent>())
                {
                    if (HamaComponent->IsDowned())
                    {
                        //HamaComponent->Revive();
                        bSuccessfullyRevivedSomeone = true;
                    }
                }
            }
        }
    }

    if (bSuccessfullyRevivedSomeone)
    {
        ResetPower();
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Medical Support Successfully Revived Player(s)!"));
    }
    else
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, TEXT("Medical Support Failed: No downed players nearby. Power kept."));
    }
}

void UHamaAbilityComponent::ActivateGhostMode()
{
    if (bIsGhost) return;

    bIsGhost = true;
    MARK_PROPERTY_DIRTY_FROM_NAME(UHamaAbilityComponent, bIsGhost, this);
    ResetPower();

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

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Purple, TEXT("Ghost Mode Activated on Server!"));

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            GhostTimerHandle,
            this,
            &UHamaAbilityComponent::DeactivateGhostMode,
            AbilityDuration,
            false
        );
    }

    OnRep_IsGhost();
}

void UHamaAbilityComponent::DeactivateGhostMode()
{
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
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Purple, TEXT("Visuals: Player became a Ghost"));
        OwnerChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

    }
    else
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("Visuals: Player returned to Normal"));

        // 🚶‍♂️ گەڕانەوە بۆ دۆخی ئاسایی: کارەکتەرەکە جارێکی تر بەر زۆمبییەکان دەکەوێتەوە
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
        ResetPower();
    }

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, TEXT("Blitz Activated on Server!"));
}


void UHamaAbilityComponent::StopAllAbilities()
{
    UWorld* World = GetWorld();
    if (!World) return;

    World->GetTimerManager().ClearAllTimersForObject(this);

    if (bIsGhost)
    {
        bIsGhost = false;

        MARK_PROPERTY_DIRTY_FROM_NAME(UHamaAbilityComponent, bIsGhost, this);

        if (GetOwner() && GetOwner()->HasAuthority())
        {
            OnRep_IsGhost();
        }
    }
}