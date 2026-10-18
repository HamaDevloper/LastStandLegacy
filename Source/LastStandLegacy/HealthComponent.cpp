#include "HealthComponent.h"
#include "Hama.h"
#include "HamaComponent.h"
#include "HamaPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "GameFramework/GameStateBase.h"
#include "LastStandLegacyGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
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

void UHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RegenerateHealthTimer);
        World->GetTimerManager().ClearTimer(DownTimerHandle);
        World->GetTimerManager().ClearTimer(QuickReviveTimerHandle);
    }

    Super::EndPlay(EndPlayReason);
}

void UHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams SharedParams;
    SharedParams.bIsPushBased = true;
    SharedParams.Condition = COND_None;

    DOREPLIFETIME_WITH_PARAMS_FAST(UHealthComponent, bIsDowned, SharedParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(UHealthComponent, bIsBeingRevived, SharedParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(UHealthComponent, BleedOutEndTime, SharedParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(UHealthComponent, PausedBleedOutRemaining, SharedParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(UHealthComponent, SavedReviveProgress, SharedParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(UHealthComponent, ReviveStateChangeTime, SharedParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(UHealthComponent, ActiveReviveRate, SharedParams);

    FDoRepLifetimeParams OwnerParams;
    OwnerParams.bIsPushBased = true;
    OwnerParams.Condition = COND_OwnerOnly;

    DOREPLIFETIME_WITH_PARAMS_FAST(UHealthComponent, CurrentHealth, OwnerParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(UHealthComponent, MaxHealth, OwnerParams);
}

float UHealthComponent::GetServerTime() const
{
    if (const UWorld* World = GetWorld())
    {
        if (const AGameStateBase* GS = World->GetGameState())
        {
            return GS->GetServerWorldTimeSeconds();
        }
        return World->GetTimeSeconds();
    }
    return 0.0f;
}

float UHealthComponent::GetBleedOutPercent() const
{
    if (!bIsDowned || DeathTime <= 0.0f) return 0.0f;

    if (bIsBeingRevived && PausedBleedOutRemaining > 0.0f)
    {
        return FMath::Clamp(PausedBleedOutRemaining / DeathTime, 0.0f, 1.0f);
    }

    const float Remaining = FMath::Max(0.0f, BleedOutEndTime - GetServerTime());
    return FMath::Clamp(Remaining / DeathTime, 0.0f, 1.0f);
}

float UHealthComponent::GetActiveRevivePercent() const
{
    if (!bIsDowned) return 0.0f;

    const float Elapsed = FMath::Max(0.0f, GetServerTime() - ReviveStateChangeTime);

    if (bIsBeingRevived)
    {
        if (ActiveReviveRate <= 0.0f) return SavedReviveProgress;
        return FMath::Clamp(SavedReviveProgress + (Elapsed * ActiveReviveRate), 0.0f, 1.0f);
    }
    else if (SavedReviveProgress > 0.0f)
    {
        const float StandardDecayRate = (ReviveDecayTime > 0.0f) ? (1.0f / ReviveDecayTime) : 0.25f;
        return FMath::Clamp(SavedReviveProgress - (Elapsed * StandardDecayRate), 0.0f, 1.0f);
    }

    return 0.0f;
}

void UHealthComponent::OnRep_CurrentHealth(float OldHealth)
{
    if (CurrentHealth < OldHealth && OwnerCharacter && OwnerCharacter->IsLocallyControlled())
    {
        // OwnerCharacter->Client_ShowDamageIndicator();
    }
}

void UHealthComponent::OnRep_IsDowned()
{
    OnDownStateChanged.Broadcast(bIsDowned);
}

void UHealthComponent::OnRep_IsBeingRevived()
{
    const bool bIsRevivingOthers = OwnerCharacter ? (OwnerCharacter->ReviveTotalDuration > 0.0f && OwnerCharacter->ReviveEndTime > 0.0f) : false;
    OnReviveStateChanged.Broadcast(bIsBeingRevived, bIsRevivingOthers);
}

void UHealthComponent::SetBeingRevived(bool bState, AHama* InReviver, float InFullReviveDuration, bool bForceResetProgress)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (bIsBeingRevived == bState && !bForceResetProgress) return;

    UWorld* World = GetWorld();
    const float ServerTime = GetServerTime();

    // ١. پێش گۆڕینی دۆخەکە، ڕێژەی ئێستای پڕۆگرێسەکە وەردەگرین (بۆ ئەوەی لەوێوە بەردەوام بێت یان دابەزێت)
    const float CurrentProgressSnapshot = bForceResetProgress ? 0.0f : GetActiveRevivePercent();

    bIsBeingRevived = bState;
    CurrentReviver = bState ? InReviver : nullptr;

    if (bForceResetProgress)
    {
        SavedReviveProgress = 0.0f;
        ReviveStateChangeTime = 0.0f;
        ActiveReviveRate = 0.0f;
    }
    else if (bState)
    {
        // دەستپێکردن یان دەستپێکردنەوەی ڕیڤایڤ
        SavedReviveProgress = CurrentProgressSnapshot;
        ReviveStateChangeTime = ServerTime;
        ActiveReviveRate = (InFullReviveDuration > 0.0f) ? (1.0f / InFullReviveDuration) : 0.25f;

        // وەستاندنی خوێنڕشتن (Pause Bleed-Out)
        if (World)
        {
            World->GetTimerManager().ClearTimer(DownTimerHandle);
        }
        PausedBleedOutRemaining = FMath::Max(0.05f, BleedOutEndTime - ServerTime);
        MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, PausedBleedOutRemaining, this);
    }
    else
    {
        // کاتێک ڕیڤایڤەکە دەپچڕێت (Cancel) - پڕۆگرێسەکە لەم خاڵەوە دەست دەکات بە دابەزین
        SavedReviveProgress = CurrentProgressSnapshot;
        ReviveStateChangeTime = ServerTime;
        // تێبینی: ActiveReviveRate وەک خۆی دەمێنێتەوە تا بە هەمان خێرایی دابەزێتەوە

        // دەستپێکردنەوەی خوێنڕشتن ڕێک لەو شوێنەی لێی وەستابوو (Resume Bleed-Out)
        if (bIsDowned && OwnerCharacter && !OwnerCharacter->bIsDead && World)
        {
            const float RemainingBleedTime = (PausedBleedOutRemaining > 0.0f)
                ? PausedBleedOutRemaining
                : FMath::Max(0.0f, BleedOutEndTime - ServerTime);

            PausedBleedOutRemaining = 0.0f;
            MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, PausedBleedOutRemaining, this);

            if (RemainingBleedTime <= 0.0f)
            {
                HandlePlayerDeath();
                return;
            }
            else
            {
                BleedOutEndTime = ServerTime + RemainingBleedTime;
                MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, BleedOutEndTime, this);
                World->GetTimerManager().SetTimer(DownTimerHandle, this, &UHealthComponent::HandlePlayerDeath, RemainingBleedTime, false);
            }
        }
    }

    MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, bIsBeingRevived, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, SavedReviveProgress, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, ReviveStateChangeTime, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, ActiveReviveRate, this);

    if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
    {
        OnRep_IsBeingRevived();
    }
}

void UHealthComponent::UpgradeHealth(float Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0.0f) return;

    MaxHealth += Amount;
    CurrentHealth = FMath::Clamp(CurrentHealth + Amount, 0.0f, MaxHealth);

    MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, MaxHealth, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, CurrentHealth, this);
}

void UHealthComponent::ApplyDamage(float Amount, AActor* DamageCauser)
{
    if (Amount <= 0.0f || !GetOwner() || !GetOwner()->HasAuthority() || bIsDowned || (OwnerCharacter && OwnerCharacter->bIsDead)) return;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RegenerateHealthTimer);

        CurrentHealth = FMath::Clamp(CurrentHealth - Amount, 0.0f, MaxHealth);
        MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, CurrentHealth, this);

        if (CurrentHealth <= 0.0f)
        {
            DownPlayer();
        }
        else
        {
            World->GetTimerManager().SetTimer(
                RegenerateHealthTimer,
                this,
                &UHealthComponent::RegenerateHealth,
                HealthTickGenerate,
                true,
                HealthGenerateDelay
            );
        }
    }
}

void UHealthComponent::RegenerateHealth()
{
    if (bIsDowned || (OwnerCharacter && OwnerCharacter->bIsDead))
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(RegenerateHealthTimer);
        }
        return;
    }

    const float HealAmountPerTick = MaxHealth / 20.0f;
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
    if (!GetOwner() || !GetOwner()->HasAuthority() || bIsDowned || (OwnerCharacter && OwnerCharacter->bIsDead)) return;

    bIsDowned = true;
    MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, bIsDowned, this);

    if (OwnerCharacter)
    {
        OwnerCharacter->CancelReviveInternal();
    }

    if (OwnerComponent)
    {
        OwnerComponent->SetDowned(true);
    }

    SetBeingRevived(false, nullptr, 0.0f, true);

    MaxHealth = 100.0f;
    CurrentHealth = 0.0f;
    MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, MaxHealth, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, CurrentHealth, this);

    if (OwnerCharacter)
    {
        OwnerCharacter->OnPlayerDowned();
    }

    if (UWorld* World = GetWorld())
    {
        FTimerManager& TM = World->GetTimerManager();
        TM.ClearTimer(RegenerateHealthTimer);
        TM.ClearTimer(DownTimerHandle);
        TM.ClearTimer(QuickReviveTimerHandle);

        ALastStandLegacyGameState* GS = World->GetGameState<ALastStandLegacyGameState>();
        if (GS && GS->bIsSoloMatch)
        {
            if (OwnerCharacter && OwnerCharacter->HasQuickRevive())
            {
                const float ServerTime = GetServerTime();
                BleedOutEndTime = ServerTime + SoloReviveTime;
                MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, BleedOutEndTime, this);

                SetBeingRevived(true, OwnerCharacter, SoloReviveTime, false);
                TM.SetTimer(QuickReviveTimerHandle, this, &UHealthComponent::Revive, SoloReviveTime, false);

                if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
                {
                    OnRep_IsDowned();
                }
                return;
            }
            else
            {
                HandlePlayerDeath();
                return;
            }
        }

        BleedOutEndTime = GetServerTime() + DeathTime;
        PausedBleedOutRemaining = 0.0f;
        MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, BleedOutEndTime, this);
        MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, PausedBleedOutRemaining, this);

        TM.SetTimer(DownTimerHandle, this, &UHealthComponent::HandlePlayerDeath, DeathTime, false);

        if (ALastStandLegacyGameMode* GM = World->GetAuthGameMode<ALastStandLegacyGameMode>())
        {
            GM->CheckGameOverCondition();
        }
    }

    if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
    {
        OnRep_IsDowned();
    }
}

void UHealthComponent::Revive()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !bIsDowned) return;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DownTimerHandle);
        World->GetTimerManager().ClearTimer(QuickReviveTimerHandle);
    }

    bIsDowned = false;
    BleedOutEndTime = 0.0f;
    PausedBleedOutRemaining = 0.0f;

    MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, bIsDowned, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, BleedOutEndTime, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, PausedBleedOutRemaining, this);

    SetBeingRevived(false, nullptr, 0.0f, true);

    if (OwnerComponent)
    {
        OwnerComponent->SetDowned(false);
    }

    CurrentHealth = MaxHealth;
    MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, CurrentHealth, this);

    if (OwnerCharacter)
    {
        OwnerCharacter->OnPlayerRevived();
    }

    if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
    {
        OnRep_IsDowned();
    }
}

void UHealthComponent::HandlePlayerDeath()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !OwnerCharacter || OwnerCharacter->bIsDead)
    {
        return;
    }

    if (bIsBeingRevived && CurrentReviver.IsValid())
    {
        return;
    }

    SetBeingRevived(false);

    BleedOutEndTime = 0.0f;
    PausedBleedOutRemaining = 0.0f;
    MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, BleedOutEndTime, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, PausedBleedOutRemaining, this);

    if (OwnerComponent)
    {
        OwnerComponent->SetDowned(false);
    }

    OwnerCharacter->SetIsDead(true);

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(QuickReviveTimerHandle);
        World->GetTimerManager().ClearTimer(DownTimerHandle);
        World->GetTimerManager().ClearTimer(RegenerateHealthTimer);
    }

    OwnerCharacter->SetActorEnableCollision(false);
    if (UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement())
    {
        MoveComp->StopActiveMovement();
        MoveComp->DisableMovement();
    }

    OnDeath.Broadcast();

    if (AHamaPlayerController* HamaPC = Cast<AHamaPlayerController>(OwnerCharacter->GetController()))
    {
        HamaPC->OnOwnerDied();
    }

    if (UWorld* World = GetWorld())
    {
        if (ALastStandLegacyGameMode* GM = World->GetAuthGameMode<ALastStandLegacyGameMode>())
        {
            GM->CheckGameOverCondition();
        }
    }

    OwnerCharacter->SetLifeSpan(3.0f);
}