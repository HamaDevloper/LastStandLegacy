#include "HamaComponent.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Hama.h"
#include "HamaMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "BaseWeapon.h"
#include "LastStandLegacyGameState.h"

UHamaComponent::UHamaComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UHamaComponent::BeginPlay()
{
    Super::BeginPlay();

    Stamina = MaxStamina;
    OwnerCharacter = Cast<AHama>(GetOwner());
    if (!OwnerCharacter) return;

    MoveComp = OwnerCharacter->FindComponentByClass<UHamaMovementComponent>();
}

void UHamaComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Params;
    Params.bIsPushBased = true;

    Params.Condition = COND_None;
    DOREPLIFETIME_WITH_PARAMS_FAST(UHamaComponent, bIsDowned, Params);

    Params.Condition = COND_SkipOwner;
    DOREPLIFETIME_WITH_PARAMS_FAST(UHamaComponent, bIsSprinting, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(UHamaComponent, bIsAiming, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(UHamaComponent, bIsSlide, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(UHamaComponent, bIsDiving, Params);
}

void UHamaComponent::SetAiming(bool bNewAiming)
{
    if (bIsAiming == bNewAiming) return;
    bIsAiming = bNewAiming;

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        MARK_PROPERTY_DIRTY_FROM_NAME(UHamaComponent, bIsAiming, this);
    }

    if (MoveComp)
    {
        MoveComp->bAiming = bIsAiming;
        if (bIsAiming)
        {
            MoveComp->bSprinting = false;
        }
    }

    if (bNewAiming)
    {
        SetSprinting(false);
    }
}

void UHamaComponent::SetDowned(bool NewValue)
{
    if (bIsDowned == NewValue) return;

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        bIsDowned = NewValue;
        MARK_PROPERTY_DIRTY_FROM_NAME(UHamaComponent, bIsDowned, this);

        if (bIsDowned && OwnerCharacter)
        {
            OwnerCharacter->CancelReviveInternal();
        }

        ApplyDownedState();
    }
}

void UHamaComponent::ApplyDownedState()
{
    if (MoveComp)
    {
        MoveComp->bDowned = bIsDowned;
    }

    if (bIsDowned)
    {
        SetSprinting(false);
        SetAiming(false);
        StopSlide();
        StopDive();
    }

    if (OwnerCharacter)
    {
        if (UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent())
        {
            const ECollisionResponse TargetResponse = bIsDowned ? ECR_Block : ECR_Ignore;
            Capsule->SetCollisionResponseToChannel(ECC_Visibility, TargetResponse);
        }
    }
}

void UHamaComponent::StartSlide()
{
    if (bIsSlide || !OwnerCharacter) return;

    bIsSlide = true;

    if (MoveComp)
    {
        MoveComp->bSlide = true;
    }

    if (OwnerCharacter->IsLocallyControlled() && GetNetMode() != NM_DedicatedServer)
    {
        if (USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh())
        {
            if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
            {
                if (OwnerCharacter->SlideMontage)
                {
                    AnimInstance->Montage_Play(OwnerCharacter->SlideMontage);
                }
            }
        }
    }
}

void UHamaComponent::StopSlide()
{
    if (!bIsSlide || !OwnerCharacter) return;

    bIsSlide = false;

    if (MoveComp)
    {
        MoveComp->bSlide = false;
    }

    if (OwnerCharacter->IsLocallyControlled() && GetNetMode() != NM_DedicatedServer)
    {
        if (USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh())
        {
            if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
            {
                if (OwnerCharacter->SlideMontage)
                {
                    AnimInstance->Montage_Stop(0.2f, OwnerCharacter->SlideMontage);
                }
            }
        }
    }
}

bool UHamaComponent::CanDive() const
{
    if (!OwnerCharacter || !MoveComp) return false;
    if (bIsDiving || bIsDowned || bIsSlide || MoveComp->IsFalling()) return false;
    if (Stamina < 25.f) return false;

    return true;
}

void UHamaComponent::StartDive()
{
    if (!CanDive()) return;

    DecreaseStamina(25.f);
    bIsDiving = true;

    if (MoveComp)
    {
        MoveComp->bDiving = true;
    }

    if (OwnerCharacter->IsLocallyControlled() && GetNetMode() != NM_DedicatedServer)
    {
        if (USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh())
        {
            if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
            {
                if (OwnerCharacter->DiveMontage)
                {
                    AnimInstance->Montage_Play(OwnerCharacter->DiveMontage);
                }
            }
        }
    }
}

void UHamaComponent::StopDive()
{
    if (!bIsDiving) return;

    bIsDiving = false;

    if (MoveComp)
    {
        MoveComp->bDiving = false;
    }

    if (OwnerCharacter && OwnerCharacter->IsLocallyControlled() && GetNetMode() != NM_DedicatedServer)
    {
        if (USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh())
        {
            if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
            {
                if (OwnerCharacter->DiveMontage)
                {
                    AnimInstance->Montage_Stop(0.2f, OwnerCharacter->DiveMontage);
                }
            }
        }
    }
}

void UHamaComponent::DecreaseStamina(float Amount)
{
    Stamina = FMath::Clamp(Stamina - Amount, 0.f, MaxStamina);
    if (!bIsSprinting) StartStaminaRegen(NormalDelayStamina);
}

void UHamaComponent::StartStaminaRegen(float Delay)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(StaminaRegenTimerHandle, this,
            &UHamaComponent::RegenerateStamina, 0.1f, true, Delay);
    }
}

void UHamaComponent::StartSprinting()
{
    SetSprinting(true);
}

void UHamaComponent::StopSprinting()
{
    SetSprinting(false);
}

void UHamaComponent::SetSprinting(bool bNewSprinting)
{
    if (bIsSprinting == bNewSprinting) return;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(StaminaRegenTimerHandle);
        World->GetTimerManager().ClearTimer(StaminaDrainTimerHandle);
    }

    bIsSprinting = bNewSprinting;

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        MARK_PROPERTY_DIRTY_FROM_NAME(UHamaComponent, bIsSprinting, this);
    }

    if (MoveComp)
    {
        MoveComp->bSprinting = bIsSprinting;
    }

    if (UWorld* World = GetWorld())
    {
        if (bIsSprinting)
        {
            SetAiming(false);
            World->GetTimerManager().ClearTimer(StaminaPenaltyTimerHandle);
            World->GetTimerManager().SetTimer(StaminaDrainTimerHandle, this, &UHamaComponent::DrainStamina, 0.1f, true);
        }
        else
        {
            if (OwnerCharacter)
            {
                OwnerCharacter->ResetValuesAfterSprint();
            }

            if (World->GetTimerManager().IsTimerActive(StaminaPenaltyTimerHandle)) return;

            World->GetTimerManager().SetTimer(StaminaRegenTimerHandle, this, &UHamaComponent::RegenerateStamina, 0.1f, true, NormalDelayStamina);
        }
    }
}

void UHamaComponent::DrainStamina()
{
    if (!OwnerCharacter || !MoveComp) return;

    if (MoveComp->IsFalling())
    {
        SetSprinting(false);
        return;
    }

    const FVector InputVector = OwnerCharacter->GetLastMovementInputVector();
    if (InputVector.SizeSquared() < 0.64f)
    {
        SetSprinting(false);
        return;
    }

    const FVector Forward2D = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();
    const FVector NormalizedInput = InputVector.GetSafeNormal2D();
    const float ForwardDot = FVector::DotProduct(Forward2D, NormalizedInput);

    if (ForwardDot < 0.5f || IsDowned())
    {
        SetSprinting(false);
        return;
    }

    if (!GSCache && GetWorld())
    {
        GSCache = GetWorld()->GetGameState<ALastStandLegacyGameState>();
    }

    const bool bInfiniteStamina = (GSCache && GSCache->IsTeamAdrenalineActive());
    if (bInfiniteStamina) return;

    if (Stamina <= 0.f)
    {
        SetSprinting(false);

        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(StaminaRegenTimerHandle);
            World->GetTimerManager().ClearTimer(StaminaDrainTimerHandle);
            World->GetTimerManager().SetTimer(
                StaminaRegenTimerHandle,
                this,
                &UHamaComponent::RegenerateStamina,
                0.1f,
                true,
                PenaltyStamina
            );
        }
        return;
    }

    Stamina = FMath::Clamp(Stamina - StaminaDrainRate * 0.1f, 0.f, MaxStamina);
}

void UHamaComponent::RegenerateStamina()
{
    if (bIsSlide) return;

    if (Stamina >= MaxStamina)
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(StaminaRegenTimerHandle);
        }
        return;
    }

    Stamina = FMath::Clamp(Stamina + StaminaRegenRate * 0.1f, 0.f, MaxStamina);
}

void UHamaComponent::UpgradeMaxStamina(float NewMaxStamina)
{
    const float StaminaRatio = (MaxStamina > 0.f) ? (Stamina / MaxStamina) : 1.0f;
    MaxStamina = NewMaxStamina;
    Stamina = MaxStamina * StaminaRatio;

    if (!bIsSprinting)
    {
        if (UWorld* World = GetWorld())
        {
            if (!World->GetTimerManager().IsTimerActive(StaminaRegenTimerHandle))
            {
                const float Delay = World->GetTimerManager().IsTimerActive(StaminaPenaltyTimerHandle) ? PenaltyStamina : NormalDelayStamina;
                World->GetTimerManager().SetTimer(
                    StaminaRegenTimerHandle, this, &UHamaComponent::RegenerateStamina,
                    0.1f, true, Delay
                );
            }
        }
    }
}

void UHamaComponent::ResetStamina()
{
    MaxStamina = 100.f;
    Stamina = 100.f;
}

void UHamaComponent::SyncStatesFromCMC(bool bInSlide, bool bInDiving, bool bInSprinting)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    if (bIsSlide != bInSlide)
    {
        bIsSlide = bInSlide;
        MARK_PROPERTY_DIRTY_FROM_NAME(UHamaComponent, bIsSlide, this);
        OnRep_Slide();
    }

    if (bIsDiving != bInDiving)
    {
        bIsDiving = bInDiving;
        MARK_PROPERTY_DIRTY_FROM_NAME(UHamaComponent, bIsDiving, this);
        OnRep_Dive();
    }

    if (bIsSprinting != bInSprinting)
    {
        bIsSprinting = bInSprinting;
        MARK_PROPERTY_DIRTY_FROM_NAME(UHamaComponent, bIsSprinting, this);
        OnRep_Sprinting();
    }
}

void UHamaComponent::OnRep_Sprinting()
{
    if (!OwnerCharacter || OwnerCharacter->IsLocallyControlled()) return;

    if (MoveComp)
    {
        MoveComp->bSprinting = bIsSprinting;
    }
}

void UHamaComponent::OnRep_Aiming()
{
    if (!OwnerCharacter || OwnerCharacter->IsLocallyControlled()) return;

    if (MoveComp)
    {
        MoveComp->bAiming = bIsAiming;
    }
}

void UHamaComponent::OnRep_Slide()
{
    if (!OwnerCharacter || OwnerCharacter->IsLocallyControlled()) return;

    if (MoveComp)
    {
        MoveComp->bSlide = bIsSlide;
    }

    if (GetNetMode() == NM_DedicatedServer) return;

    USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh();
    if (!Mesh) return;

    UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
    if (!AnimInstance || !OwnerCharacter->SlideMontage) return;

    if (bIsSlide)
    {
        AnimInstance->Montage_Play(OwnerCharacter->SlideMontage);
    }
    else
    {
        AnimInstance->Montage_Stop(0.2f, OwnerCharacter->SlideMontage);
    }
}

void UHamaComponent::OnRep_IsDowned()
{
    if (!OwnerCharacter) return;

    ApplyDownedState();

    if (MoveComp)
    {
        MoveComp->bDowned = bIsDowned;
        if (bIsDowned)
        {
            MoveComp->bSprinting = false;
            MoveComp->bAiming = false;
            MoveComp->bSlide = false;
            MoveComp->bDiving = false;
        }
    }
}

void UHamaComponent::OnRep_Dive()
{
    if (!OwnerCharacter || OwnerCharacter->IsLocallyControlled()) return;

    if (MoveComp)
    {
        MoveComp->bDiving = bIsDiving;
    }

    if (GetNetMode() == NM_DedicatedServer) return;

    USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh();
    if (!Mesh) return;

    UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
    if (!AnimInstance || !OwnerCharacter->DiveMontage) return;

    if (bIsDiving)
    {
        AnimInstance->Montage_Play(OwnerCharacter->DiveMontage);
    }
    else
    {
        AnimInstance->Montage_Stop(0.2f, OwnerCharacter->DiveMontage);
    }
}