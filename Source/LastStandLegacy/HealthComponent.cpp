#include "HealthComponent.h"
#include "Hama.h"
#include "HamaComponent.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "LastStandLegacyGameState.h"

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

    Params.Condition = COND_OwnerOnly;
    DOREPLIFETIME_WITH_PARAMS_FAST(UHealthComponent, CurrentHealth, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(UHealthComponent, MaxHealth, Params);
}

void UHealthComponent::OnRep_CurrentHealth(float OldHealth)
{
    if (CurrentHealth < OldHealth)
    {
        if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
        {
            //OwnerCharacter->Client_ShowDamageIndicator();
        }
    }
}

void UHealthComponent::UpgradeHealth(float Amount)
{
    if (!GetOwner()->HasAuthority()) return;

    MaxHealth = Amount;

    MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, MaxHealth, this);

    if (UWorld* World = GetWorld())
    {
        if (!World->GetTimerManager().IsTimerActive(RegenerateHealthTimer))
        {
            World->GetTimerManager().SetTimer(RegenerateHealthTimer, this, &UHealthComponent::RegenerateHealth, HealthTickGenerate, true, HealthGenerateDelay);
        }
    }
}

void UHealthComponent::ApplyDamage(float Amount, AActor* DamageCauser)
{
    if (!GetOwner()->HasAuthority()) return;

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

    CurrentHealth += HealAmountPerTick;

    MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, CurrentHealth, this);

    if (CurrentHealth >= MaxHealth)
    {
        CurrentHealth = MaxHealth;
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(RegenerateHealthTimer);
        }
    }
}

void UHealthComponent::DownPlayer()
{
    if (!GetOwner()->HasAuthority()) return;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RegenerateHealthTimer);
        World->GetTimerManager().ClearTimer(DownTimerHandle);

        MaxHealth = 100.f;
        CurrentHealth = MaxHealth;

        MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, CurrentHealth, this);
        MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, MaxHealth, this);
        
        if (OwnerComponent)
        {
            OwnerComponent->SetDowned(true);
        }

        ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>();
        if (GS && GS->bIsSoloMatch && OwnerCharacter && OwnerCharacter->HasQuickRevive())
        {
            OwnerCharacter->HandleDeath();
            World->GetTimerManager().SetTimer(QuickReviveTimerHandle, this, &UHealthComponent::Revive, 5.0f, false);
            return;
        }
        else
        {
            if (OwnerCharacter)
            {
                OwnerCharacter->HandleDeath();
            }
        }
      
        World->GetTimerManager().SetTimer(DownTimerHandle, this, &UHealthComponent::HandlePlayerDeath, 45.0f, false);
    }
}

void UHealthComponent::Revive()
{
    if (!GetOwner()->HasAuthority()) return;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DownTimerHandle);
    }

    CurrentHealth = MaxHealth;
    
    MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, CurrentHealth, this);

    if (OwnerComponent)
    {
        OwnerComponent->SetDowned(false);
    }
}

void UHealthComponent::HandlePlayerDeath()
{
    if (!GetOwner()->HasAuthority() || !OwnerCharacter || OwnerCharacter->bIsDead) return;

    GetWorld()->GetTimerManager().ClearTimer(QuickReviveTimerHandle);

    OnDeath.Broadcast();

    APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
    if (PC && GetWorld() && GetWorld()->GetGameState())
    {
        PC->UnPossess();

        APlayerController* SpectatorPC = nullptr;
        for (auto PlayerState : GetWorld()->GetGameState()->PlayerArray)
        {
            if (PlayerState && PlayerState->GetPawn() && PlayerState->GetPawn() != OwnerCharacter)
            {
                SpectatorPC = Cast<APlayerController>(PlayerState->GetOwner());
                break;
            }
        }

        PC->ChangeState(NAME_Spectating);
        PC->ClientGotoState(NAME_Spectating);

        if (SpectatorPC && SpectatorPC->GetPawn())
        {
            PC->SetViewTargetWithBlend(SpectatorPC->GetPawn(), 0.5f);
        }
    }

    OwnerCharacter->Destroy();
}