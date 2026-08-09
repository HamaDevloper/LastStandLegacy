#include "HealthComponent.h"
#include "Hama.h"
#include "HamaComponent.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "LastStandLegacyGameState.h"
#include "ZombieDirectorSubsystem.h"

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
    DOREPLIFETIME_WITH_PARAMS_FAST(UHealthComponent, bIsBeingRevived, Params);
}

bool UHealthComponent::IsDowned() const
{
    return OwnerComponent ? OwnerComponent->IsDowned() : false;
}

void UHealthComponent::OnRep_CurrentHealth(float OldHealth)
{
    if (CurrentHealth < OldHealth)
    {
        if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
        {
            // OwnerCharacter->Client_ShowDamageIndicator();
        }
    }
}

void UHealthComponent::OnRep_IsBeingRevived()
{
    // 📢 لۆجیکی کڵاینت کاتێک یاریزانەکە دەست دەکرێت بە ڕزگارکردنی (Reviving)
    // نموونە: ئاگادارکردنەوەی UI یان لێدانی ئەنیمەیشن/دەنگ

    if (OnReviveStateChanged.IsBound())
    {
        OnReviveStateChanged.Broadcast(bIsBeingRevived);
    }
}

void UHealthComponent::UpgradeHealth(float Amount)
{
    if (!GetOwner()->HasAuthority() || Amount <= 0.0f) return;

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

    CurrentHealth += HealAmountPerTick;

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

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RegenerateHealthTimer);
        World->GetTimerManager().ClearTimer(DownTimerHandle);
        World->GetTimerManager().ClearTimer(QuickReviveTimerHandle);

        MaxHealth = 100.f;
        CurrentHealth = MaxHealth;

        MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, CurrentHealth, this);
        MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, MaxHealth, this);


        if (OwnerCharacter)
        {
            OwnerCharacter->HandleDeath();
        }

        if (auto* Director = World->GetSubsystem<UZombieDirectorSubsystem>())
        {
            Director->SetPlayerTargetable(Cast<APawn>(GetOwner()), false);
        }

        ALastStandLegacyGameState* GS = World->GetGameState<ALastStandLegacyGameState>();
        if (GS && GS->bIsSoloMatch && OwnerCharacter && OwnerCharacter->HasQuickRevive())
        {
            World->GetTimerManager().SetTimer(QuickReviveTimerHandle, this, &UHealthComponent::Revive, SoloReviveTime, false);
            return;
        }

        World->GetTimerManager().SetTimer(DownTimerHandle, this, &UHealthComponent::HandlePlayerDeath, DeathTime, false);
    }
}

void UHealthComponent::Revive()
{
    if (!GetOwner()->HasAuthority()) return;

    bIsBeingRevived = false;

    if (OwnerComponent)
    {
        OwnerComponent->SetDowned(false);
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DownTimerHandle);
        World->GetTimerManager().ClearTimer(QuickReviveTimerHandle);

        if (auto* Director = World->GetSubsystem<UZombieDirectorSubsystem>())
        {
            Director->SetPlayerTargetable(Cast<APawn>(GetOwner()), true);
        }
    }

    CurrentHealth = MaxHealth;
    MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, CurrentHealth, this);
}

void UHealthComponent::HandlePlayerDeath()
{
    if (!GetOwner()->HasAuthority() || !OwnerCharacter || OwnerCharacter->bIsDead) return;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(QuickReviveTimerHandle);
        World->GetTimerManager().ClearTimer(DownTimerHandle);
    }

    OnDeath.Broadcast();

    APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
    if (PC && GetWorld() && GetWorld()->GetGameState())
    {
        APawn* SpectatorTargetPawn = nullptr;
        for (auto PlayerState : GetWorld()->GetGameState()->PlayerArray)
        {
            if (PlayerState && PlayerState->GetPawn() && PlayerState->GetPawn() != OwnerCharacter)
            {
                SpectatorTargetPawn = PlayerState->GetPawn();
                break;
            }
        }

        PC->UnPossess();

        if (SpectatorTargetPawn)
        {
            PC->SetViewTargetWithBlend(SpectatorTargetPawn, 0.5f);
        }

        PC->ChangeState(NAME_Spectating);
        PC->ClientGotoState(NAME_Spectating);
    }

    OwnerCharacter->Destroy();
}