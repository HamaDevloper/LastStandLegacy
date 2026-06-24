#include "HamaAbilityComponent.h"
#include "Net/UnrealNetwork.h"
#include "Hama.h"
#include "LastStandLegacyGameState.h"
//#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"

UHamaAbilityComponent::UHamaAbilityComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UHamaAbilityComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UHamaAbilityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UHamaAbilityComponent, CurrentPower, COND_OwnerOnly);
    DOREPLIFETIME(UHamaAbilityComponent, bIsGhost);
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
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Magenta, FString::Printf(TEXT("Current Power is: %f"), CurrentPower));
    }
    if (GetOwner()->GetLocalRole() == ROLE_Authority && GetNetMode() != NM_DedicatedServer)
    {
        OnRep_CurrentPower();
    }
}

bool UHamaAbilityComponent::IsPowerFull() const
{
    return CurrentPower >= MaxPower;
}

void UHamaAbilityComponent::OnRep_CurrentPower()
{
    OnPowerChanged.Broadcast(CurrentPower);
}

void UHamaAbilityComponent::Server_ActivateAbility_Implementation()
{
    if (CurrentPower < MaxPower) return;

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
    case EHamaAbilityType::Decoy:
        ActivateDecoy();
        break;
    default:
        UE_LOG(LogTemp, Warning, TEXT("Player tried to activate ability but has NONE assigned!"));
        break;
    }
    CurrentPower = 0.f;
}

void UHamaAbilityComponent::ActivateBulletStorm()
{
    ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>();
    if (GS)
    {
        GS->StartGlobalBulletStorm(AbilityDuration);
    }
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("BulletStorm Activated on Server!"));
}

void UHamaAbilityComponent::ActivateMedicalSupport()
{
    AActor* Owner = GetOwner();
    if (!Owner) return;

    UWorld* World = GetWorld();
    if (!World) return;

    if (!Owner->HasAuthority()) return;

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Medical Support Activated on Server!"));

    FVector StartLocation = Owner->GetActorLocation();
    FVector EndLocation = StartLocation;

    TArray<FHitResult> HitResults;
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(SphereRadius);

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Owner);

    bool bHit = World->SweepMultiByChannel(HitResults, StartLocation, EndLocation, FQuat::Identity, ECC_Pawn, SphereShape, Params);

    if (bHit)
    {
        for (const FHitResult& Hit : HitResults)
        {
            if (AHama* Hama = Cast<AHama>(Hit.GetActor()))
            {
                if (UHamaComponent* HamaComponent = Hama->FindComponentByClass<UHamaComponent>())
                {
                    if (HamaComponent->IsDowned())
                    {
                        HamaComponent->Revive();
                    }
                }
            }
        }
    }
}

void UHamaAbilityComponent::ActivateGhostMode()
{
    AActor* Owner = GetOwner();

    // ئەگەر خاوەنی نەبوو، یان ئەگەر کۆدەکە لەسەر کڵایەنت ڕەن بوو، ڕاستەوخۆ بیوەستێنە
    if (!Owner || !Owner->HasAuthority())
    {
        return;
    }

    // ئەگەر پێشتر خێو بوو، پێویست ناکات دووبارە چالاکی بکەینەوە
    if (bIsGhost) return;

    bIsGhost = true;

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Purple, TEXT("Ghost Mode Activated on Server!"));

    // دانانی تایمەر بۆ کوژاندنەوەی مۆدی خێو پاش تەواوبوونی کاتەکەی
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

    // تێبینی: فەنکشنی OnRep لەسەر سێرڤەر خۆکارانە بانگ ناکرێت، بۆیە دەبێت بە دەست بانگی بکەین
    // بۆ ئەوەی سێرڤەریش گۆڕانکارییە بینراوەکانی بەسەردا بێت
    OnRep_IsGhost();
}

void UHamaAbilityComponent::ActivateDecoy()
{
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, TEXT("Decoy Activated on Server!"));
}

void UHamaAbilityComponent::DeactivateGhostMode()
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority()) return;

    bIsGhost = false;

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Ghost Mode Deactivated!"));

    OnRep_IsGhost();
}

void UHamaAbilityComponent::OnRep_IsGhost()
{
    // ئەم بەشە لەسەر هەموو کڵایەنتەکان و سێرڤەرەکەش ڕەن دەبێت کاتێک bIsGhost دەگۆڕێت
    if (bIsGhost)
    {
        // لێرەدا دەتوانیت کۆدی گۆڕینی مەتێریاڵ (Material) یان شاردنەوەی چەکی یاریزانەکە بنووسیت
        // بۆ نموونە: یاریزانەکە بکەیتە شوشەیی
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Purple, TEXT("Visuals: Player became a Ghost"));
    }
    else
    {
        // لێرەدا یاریزانەکە دەگەڕێنیتەوە باری ئاسایی خۆی
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("Visuals: Player returned to Normal"));
    }
}

void UHamaAbilityComponent::StopAllAbilities()
{
    UWorld* World = GetWorld();
    if (!World) return;

    World->GetTimerManager().ClearAllTimersForObject(this);

    if (bIsGhost)
    {
        bIsGhost = false;
        if (GetOwner() && GetOwner()->HasAuthority())
        {
            OnRep_IsGhost();
        }
    }
}