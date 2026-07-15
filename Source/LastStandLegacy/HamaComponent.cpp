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

    Params.Condition = COND_SkipOwner;
    DOREPLIFETIME_WITH_PARAMS_FAST(UHamaComponent, bIsSprinting, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(UHamaComponent, bIsAiming, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(UHamaComponent, bIsSlide, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(UHamaComponent, bIsDowned, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(UHamaComponent, bIsDiving, Params);

    Params.Condition = COND_OwnerOnly;
    DOREPLIFETIME_WITH_PARAMS_FAST(UHamaComponent, MaxStamina, Params);
}

void UHamaComponent::SetAiming(bool bNewAiming)
{
    if (bIsAiming == bNewAiming) return;
    bIsAiming = bNewAiming;

    MARK_PROPERTY_DIRTY_FROM_NAME(UHamaComponent, bIsAiming, this);

    if (MoveComp) MoveComp->bAiming = bIsAiming;
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
}

void UHamaComponent::StartSlide()
{
    if (bIsSlide || !OwnerCharacter) return;

    bIsSlide = true;
    if (MoveComp) MoveComp->bSlide = true;

    if (!OwnerCharacter->HasAuthority())
    {
        Server_SetSlideState(true);
    }
    else
    {
        MARK_PROPERTY_DIRTY_FROM_NAME(UHamaComponent, bIsSlide, this);
    }
}

void UHamaComponent::Server_SetSlideState_Implementation(bool bNewSlideState)
{
    if (bIsSlide == bNewSlideState) return;
    bIsSlide = bNewSlideState;

    MARK_PROPERTY_DIRTY_FROM_NAME(UHamaComponent, bIsSlide, this);

    if (GetNetMode() == NM_ListenServer)
    {
        OnRep_Slide();
    }
}

void UHamaComponent::StopSlide()
{
    if (!bIsSlide || !OwnerCharacter) return;

    bIsSlide = false;
    if (MoveComp) MoveComp->bSlide = false;

    if (!OwnerCharacter->HasAuthority())
    {
        Server_SetSlideState(false);
    }
    else
    {
        MARK_PROPERTY_DIRTY_FROM_NAME(UHamaComponent, bIsSlide, this);
    }
}

void UHamaComponent::StartDive()
{
    if (bIsDiving || !OwnerCharacter) return;

    FVector DiveDirection = OwnerCharacter->GetActorForwardVector() * 900.f;
    DiveDirection.Z = 350.f;

    if (OwnerCharacter->HasAuthority())
    {
        if (Stamina < 25.f) return;

        bIsDiving = true;
        DecreaseStamina(25.f);

        OwnerCharacter->LaunchCharacter(DiveDirection, true, true);
        MARK_PROPERTY_DIRTY_FROM_NAME(UHamaComponent, bIsDiving, this);
    }
    else
    {
        bIsDiving = true;
        OwnerCharacter->LaunchCharacter(DiveDirection, true, true);
        Server_SetDiving(true);
    }
}

void UHamaComponent::StopDive()
{
    if (!bIsDiving || !OwnerCharacter) return;
    bIsDiving = false;
    if (!OwnerCharacter->HasAuthority())
    {
        Server_SetDiving(false);
    }
    else
    {
        MARK_PROPERTY_DIRTY_FROM_NAME(UHamaComponent, bIsDiving, this);
    }
}

void UHamaComponent::Server_SetDiving_Implementation(bool bNewDiveState)
{
    if (bIsDiving == bNewDiveState) return;

    if (bNewDiveState)
    {
        if (Stamina < 25.f)
        {
            Client_RejectDive();
            return;
        }
        DecreaseStamina(25.f);

        FVector DiveDirection = OwnerCharacter->GetActorForwardVector() * 900.f;
        DiveDirection.Z = 350.f;
        OwnerCharacter->LaunchCharacter(DiveDirection, true, true);
    }

    bIsDiving = bNewDiveState;
    MARK_PROPERTY_DIRTY_FROM_NAME(UHamaComponent, bIsDiving, this);

    if (GetNetMode() == NM_ListenServer)
    {
        OnRep_Dive();
    }
}

void UHamaComponent::Client_RejectDive_Implementation()
{
    bIsDiving = false;
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

    MARK_PROPERTY_DIRTY_FROM_NAME(UHamaComponent, bIsSprinting, this);

    if (OwnerCharacter) OwnerCharacter->ForceNetUpdate();
    if (MoveComp) MoveComp->bSprinting = bIsSprinting;

    if (bIsSprinting)
    {
        GetWorld()->GetTimerManager().ClearTimer(StaminaPenaltyTimerHandle);
        GetWorld()->GetTimerManager().SetTimer(StaminaDrainTimerHandle, this, &UHamaComponent::DrainStamina, 0.1f, true);
    }
    else
    {
        if (GetWorld()->GetTimerManager().IsTimerActive(StaminaPenaltyTimerHandle)) return;
        GetWorld()->GetTimerManager().SetTimer(StaminaRegenTimerHandle, this, &UHamaComponent::RegenerateStamina, 0.1f, true, NormalDelayStamina);
    }
}

void UHamaComponent::DrainStamina()
{
    if (!OwnerCharacter || !MoveComp) return;
    if (!GSCache)
    {
        GSCache = GetWorld()->GetGameState<ALastStandLegacyGameState>();
    }
    if (GSCache && GSCache->IsTeamAdrenalineActive()) return;

    if (Stamina <= 0.f)
    {
        GetWorld()->GetTimerManager().SetTimer(StaminaPenaltyTimerHandle, PenaltyStamina, false);
        SetSprinting(false);
        GetWorld()->GetTimerManager().SetTimer(StaminaRegenTimerHandle, this, &UHamaComponent::RegenerateStamina, 0.1f, true, PenaltyStamina);
        return;
    }

    if (MoveComp->IsFalling()) return;

    FVector InputVector = OwnerCharacter->GetLastMovementInputVector();
    if (InputVector.IsNearlyZero())
    {
        SetSprinting(false);
        return;
    }

    FVector CurrentVelocity = OwnerCharacter->GetVelocity();
    CurrentVelocity.Z = 0.f;

    if (CurrentVelocity.SizeSquared() < 10000.f) return;

    FVector ForwardVector = OwnerCharacter->GetActorForwardVector();
    InputVector.Normalize();
    float ForwardMotion = FVector::DotProduct(InputVector, ForwardVector);

    if (ForwardMotion < 0.5f)
    {
        SetSprinting(false);
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

    MARK_PROPERTY_DIRTY_FROM_NAME(UHamaComponent, MaxStamina, this);

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

    MARK_PROPERTY_DIRTY_FROM_NAME(UHamaComponent, MaxStamina, this);
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
    if (OwnerCharacter && !OwnerCharacter->IsLocallyControlled())
    {
        USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh();
        if (!Mesh) return;

        UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
        if (!AnimInstance || !OwnerCharacter->DiveMontage) return;

        if (bIsDiving) AnimInstance->Montage_Play(OwnerCharacter->DiveMontage);
        else           AnimInstance->Montage_Stop(0.2f, OwnerCharacter->DiveMontage);
    }
}