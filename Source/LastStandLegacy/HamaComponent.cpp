#include "HamaComponent.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Hama.h"
#include "HamaMovementComponent.h"
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
    DOREPLIFETIME_WITH_PARAMS_FAST(UHamaComponent, bIsSprinting, Params);

    Params.Condition = COND_SkipOwner;
    DOREPLIFETIME_WITH_PARAMS_FAST(UHamaComponent, bIsAiming, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(UHamaComponent, bIsSlide, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(UHamaComponent, bIsDiving, Params);
}

void UHamaComponent::SetAiming(bool bNewAiming)
{
    if (bIsAiming == bNewAiming) return;
    bIsAiming = bNewAiming;

    if (MoveComp) MoveComp->bAiming = bIsAiming;

    if (bNewAiming)
    {
        SetSprinting(false);
    }
}

void UHamaComponent::SetDowned(bool NewValue)
{
    if (bIsDowned == NewValue) return;
    bIsDowned = NewValue;

    if (OwnerCharacter && OwnerCharacter->HasAuthority())
    {
        MARK_PROPERTY_DIRTY_FROM_NAME(UHamaComponent, bIsDowned, this);
    }

    if (MoveComp) MoveComp->bDowned = bIsDowned;

    if (bIsDowned)
    {
        OwnerCharacter->Server_CancelRevive();
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

    if (OwnerCharacter->HasAuthority())
    {
        MARK_PROPERTY_DIRTY_FROM_NAME(UHamaComponent, bIsSlide, this);
    }
}

void UHamaComponent::StopSlide()
{
    if (!bIsSlide || !OwnerCharacter) return;

    bIsSlide = false;

    if (MoveComp)
    {
        MoveComp->bSlide = bIsSlide;
    }

    if (OwnerCharacter->HasAuthority())
    {
        MARK_PROPERTY_DIRTY_FROM_NAME(UHamaComponent, bIsSlide, this);
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

    bIsDiving = true;

    if (MoveComp)
    {
        MoveComp->bDiving = bIsDiving;
    }

    if (OwnerCharacter->HasAuthority())
    {
        MARK_PROPERTY_DIRTY_FROM_NAME(UHamaComponent, bIsDiving, this);
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

    if (OwnerCharacter && OwnerCharacter->HasAuthority())
    {
        MARK_PROPERTY_DIRTY_FROM_NAME(UHamaComponent, bIsDiving, this);
    }
}


void UHamaComponent::DecreaseStamina(float Amount)
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        Stamina = FMath::Clamp(Stamina - Amount, 0.0f, MaxStamina);
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

    GetWorld()->GetTimerManager().ClearTimer(StaminaRegenTimerHandle);
    GetWorld()->GetTimerManager().ClearTimer(StaminaDrainTimerHandle);

    bIsSprinting = bNewSprinting;

    if (MoveComp) MoveComp->bSprinting = bIsSprinting;

    if (bIsSprinting)
    {
        SetAiming(false);
        GetWorld()->GetTimerManager().ClearTimer(StaminaPenaltyTimerHandle);
        GetWorld()->GetTimerManager().SetTimer(StaminaDrainTimerHandle, this, &UHamaComponent::DrainStamina, 0.1f, true);
    }
    else
    {
        if (OwnerCharacter) OwnerCharacter->ResetValuesAfterSprint();
        if (GetWorld()->GetTimerManager().IsTimerActive(StaminaPenaltyTimerHandle)) return;
        GetWorld()->GetTimerManager().SetTimer(StaminaRegenTimerHandle, this, &UHamaComponent::RegenerateStamina, 0.1f, true, NormalDelayStamina);
    }
}


void UHamaComponent::DrainStamina()
{
    if (!OwnerCharacter || !MoveComp) return;

    if (MoveComp->IsFalling()) return;

    FVector InputVector = OwnerCharacter->GetLastMovementInputVector();
    if (InputVector.SizeSquared() < 0.1f)
    {
        SetSprinting(false);
        return;
    }

    FVector ForwardVector = OwnerCharacter->GetActorForwardVector();
    FVector NormalizedInput = InputVector.GetSafeNormal2D();

    float ForwardDot = FVector::DotProduct(ForwardVector, NormalizedInput);

    if (ForwardDot < 0.5f)
    {
        SetSprinting(false);
        return;
    }

    FVector CurrentVelocity = OwnerCharacter->GetVelocity();
    CurrentVelocity.Z = 0.f;

    if (CurrentVelocity.SizeSquared() < FMath::Square(750.f))
    {
        SetSprinting(false);
        return;
    }

    if (!GSCache)
    {
        GSCache = GetWorld()->GetGameState<ALastStandLegacyGameState>();
    }

    const bool bInfiniteStamina = (GSCache && GSCache->IsTeamAdrenalineActive());
    if (bInfiniteStamina) return;

    if (Stamina <= 0.f)
    {
        SetSprinting(false);

        GetWorld()->GetTimerManager().ClearTimer(StaminaRegenTimerHandle);

        GetWorld()->GetTimerManager().SetTimer(
            StaminaRegenTimerHandle,
            this,
            &UHamaComponent::RegenerateStamina,
            0.1f,
            true,
            PenaltyStamina
        );
        return;
    }

    Stamina = FMath::Clamp(Stamina - StaminaDrainRate * 0.1f, 0.f, MaxStamina);
}

void UHamaComponent::RegenerateStamina()
{
    if (bIsSlide) return;
    if (Stamina >= MaxStamina)
    {
        GetWorld()->GetTimerManager().ClearTimer(StaminaRegenTimerHandle);
        return;
    }

    Stamina = FMath::Clamp(Stamina + StaminaRegenRate * 0.1f, 0.f, MaxStamina);
}

void UHamaComponent::UpgradeMaxStamina(float NewMaxStamina)
{
    float StaminaRatio = (MaxStamina > 0.f) ? (Stamina / MaxStamina) : 1.0f;

    MaxStamina = NewMaxStamina;

    Stamina = MaxStamina * StaminaRatio;

    if (!bIsSprinting)
    {
        if (GetWorld() && !GetWorld()->GetTimerManager().IsTimerActive(StaminaRegenTimerHandle))
        {
            float Delay = GetWorld()->GetTimerManager().IsTimerActive(StaminaPenaltyTimerHandle) ? PenaltyStamina : NormalDelayStamina;

            GetWorld()->GetTimerManager().SetTimer(
                StaminaRegenTimerHandle, this, &UHamaComponent::RegenerateStamina,
                0.1f, true, Delay
            );
        }
    }
}

void UHamaComponent::ResetStamina()
{
    MaxStamina = 100.f;
    Stamina = 100.f;
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

    USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh();
    if (!Mesh) return;

    UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
    if (!AnimInstance || !OwnerCharacter->SlideMontage) return;

    if (bIsSlide) AnimInstance->Montage_Play(OwnerCharacter->SlideMontage);
    else          AnimInstance->Montage_Stop(0.2f, OwnerCharacter->SlideMontage);
}

void UHamaComponent::OnRep_Down()
{
    if (!OwnerCharacter || OwnerCharacter->IsLocallyControlled()) return;
    if (MoveComp)
    {
        MoveComp->bDowned = bIsDowned;
    }
}

void UHamaComponent::OnRep_Dive()
{
    if (!OwnerCharacter || OwnerCharacter->IsLocallyControlled()) return;

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