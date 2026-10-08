#include "HealthComponent.h"
#include "Hama.h"
#include "HamaComponent.h"
#include "HamaPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "GameFramework/GameStateBase.h"
#include "LastStandLegacyGameMode.h" 

UHealthComponent::UHealthComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UHealthComponent::BeginPlay()
{
    Super::BeginPlay();

    OwnerCharacter = Cast<AHama>(GetOwner());
    if (OwnerCharacter)
    {
        OwnerComponent = OwnerCharacter->FindComponentByClass<UHamaComponent>();
    }
}

void UHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Params;
    Params.bIsPushBased = true;

    Params.Condition = COND_None;
    DOREPLIFETIME_WITH_PARAMS_FAST(UHealthComponent, bIsBeingRevived, Params);

    Params.Condition = COND_OwnerOnly;
    DOREPLIFETIME_WITH_PARAMS_FAST(UHealthComponent, CurrentHealth, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(UHealthComponent, MaxHealth, Params);
}

bool UHealthComponent::IsDowned() const
{
    return OwnerComponent ? OwnerComponent->IsDowned() : false;
}

void UHealthComponent::OnRep_CurrentHealth(float OldHealth)
{
    if (CurrentHealth < OldHealth && OwnerCharacter && OwnerCharacter->IsLocallyControlled())
    {
        // OwnerCharacter->Client_ShowDamageIndicator();
    }
}

void UHealthComponent::OnRep_IsBeingRevived()
{
    OnReviveStateChanged.Broadcast(bIsBeingRevived);
}

void UHealthComponent::UpgradeHealth(float Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0.0f) return;

    MaxHealth += Amount;
    CurrentHealth = FMath::Clamp(CurrentHealth + Amount, 0.f, MaxHealth);

    MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, MaxHealth, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, CurrentHealth, this);
}

void UHealthComponent::ApplyDamage(float Amount, AActor* DamageCauser)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || IsDowned()) return;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RegenerateHealthTimer);

        CurrentHealth = FMath::Clamp(CurrentHealth - Amount, 0.f, MaxHealth);
        MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, CurrentHealth, this);

        if (CurrentHealth <= 0.f)
        {
            DownPlayer();
        }
        else
        {
            World->GetTimerManager().SetTimer(RegenerateHealthTimer, this, &UHealthComponent::RegenerateHealth, HealthTickGenerate, true, HealthGenerateDelay);
        }
    }
}

void UHealthComponent::RegenerateHealth()
{
    float HealAmountPerTick = MaxHealth / 20.0f;
    CurrentHealth = FMath::Min(CurrentHealth + HealAmountPerTick, MaxHealth);

    if (CurrentHealth >= MaxHealth)
    {
        CurrentHealth = MaxHealth;
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(RegenerateHealthTimer);
        }
    }

    MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, CurrentHealth, this);
}

void UHealthComponent::DownPlayer()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || IsDowned()) return;

    bIsBeingRevived = false;
    CurrentReviver = nullptr;
    MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, bIsBeingRevived, this);

    if (OwnerComponent)
    {
        OwnerComponent->SetDowned(true);
    }

    OnDownStateChanged.ExecuteIfBound(true);

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RegenerateHealthTimer);
        World->GetTimerManager().ClearTimer(DownTimerHandle);
        World->GetTimerManager().ClearTimer(QuickReviveTimerHandle);

        MaxHealth = 100.f;
        CurrentHealth = MaxHealth;

        MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, CurrentHealth, this);
        MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, MaxHealth, this);

        if (ALastStandLegacyGameMode* GM = World->GetAuthGameMode<ALastStandLegacyGameMode>())
        {
            GM->CheckGameOverCondition();
        }

        AGameStateBase* GS = World->GetGameState();

        if (GS && OwnerCharacter && OwnerCharacter->HasQuickRevive() /* و بەستنەوە بە Solo Check لە GameMode/GameState */)
        {
            World->GetTimerManager().SetTimer(QuickReviveTimerHandle, this, &UHealthComponent::Revive, SoloReviveTime, false);
            return;
        }

        World->GetTimerManager().SetTimer(DownTimerHandle, this, &UHealthComponent::HandlePlayerDeath, DeathTime, false);
    }
}

void UHealthComponent::Revive()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    bIsBeingRevived = false;
    MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, bIsBeingRevived, this);

    if (OwnerComponent)
    {
        OwnerComponent->SetDowned(false);
    }

    OnDownStateChanged.ExecuteIfBound(false);

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DownTimerHandle);
        World->GetTimerManager().ClearTimer(QuickReviveTimerHandle);
    }

    CurrentHealth = MaxHealth;
    MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, CurrentHealth, this);
}

void UHealthComponent::HandlePlayerDeath()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !OwnerCharacter || OwnerCharacter->bIsDead)
    {
        return;
    }

    OwnerCharacter->bIsDead = true;
    MARK_PROPERTY_DIRTY_FROM_NAME(AHama, bIsDead, OwnerCharacter);

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(QuickReviveTimerHandle);
        World->GetTimerManager().ClearTimer(DownTimerHandle);
    }

    OnDeath.Broadcast();

    if (APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController()))
    {
        if (AHamaPlayerController* HamaPC = Cast<AHamaPlayerController>(PC))
        {
            HamaPC->OnOwnerDied(); 
        }
    }

    if (UWorld* World = GetWorld())
    {
        if (ALastStandLegacyGameMode* GM = World->GetAuthGameMode<ALastStandLegacyGameMode>())
        {
            GM->CheckGameOverCondition();
        }
    }

    OwnerCharacter->Destroy();
}