#include "HamaComponent.h"
#include "Net/UnrealNetwork.h"
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

    DOREPLIFETIME_CONDITION(UHamaComponent, bIsSprinting, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(UHamaComponent, bIsAiming, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(UHamaComponent, bIsSlide, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(UHamaComponent, bIsDowned, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(UHamaComponent, bIsDiving, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(UHamaComponent, MaxStamina, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UHamaComponent, Stamina, COND_OwnerOnly);
}

void UHamaComponent::SetAiming(bool bNewAiming)
{
    if (bIsAiming == bNewAiming) return;
    bIsAiming = bNewAiming;
    OwnerCharacter->ForceNetUpdate();
    if (MoveComp) MoveComp->bAiming = bIsAiming;
}

void UHamaComponent::SetDown(bool NewValue)
{
    if (bIsDowned == NewValue) return;
    bIsDowned = NewValue;
    OwnerCharacter->ForceNetUpdate();
    if (MoveComp) MoveComp->bDowned = bIsDowned;
}

void UHamaComponent::StartSlide()
{
    if (bIsSlide || !OwnerCharacter) return;

    bIsSlide = true;

    FVector SlideDirection = OwnerCharacter->GetActorForwardVector() * 800.f;
    OwnerCharacter->LaunchCharacter(SlideDirection, true, false);

    if (!OwnerCharacter->HasAuthority())
    {
        Server_SetSlideState(true);
    }
    else
    {
        OwnerCharacter->ForceNetUpdate();
    }
}

void UHamaComponent::StopSlide()
{
    if (!bIsSlide || !OwnerCharacter) return;

    bIsSlide = false;
    if (!OwnerCharacter->HasAuthority())
    {
        Server_SetSlideState(false);
    }
    else
    {
        OwnerCharacter->ForceNetUpdate();
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
        OwnerCharacter->ForceNetUpdate();
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
        OwnerCharacter->ForceNetUpdate();
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
    if (OwnerCharacter) OwnerCharacter->ForceNetUpdate();

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

void UHamaComponent::Server_SetSlideState_Implementation(bool bNewSlideState)
{
    if (bIsSlide == bNewSlideState) return;
    bIsSlide = bNewSlideState;

    if (OwnerCharacter) OwnerCharacter->ForceNetUpdate();

    if (GetNetMode() == NM_ListenServer)
    {
        OnRep_Slide();
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
    if (OwnerCharacter) OwnerCharacter->ForceNetUpdate();
    if (MoveComp) MoveComp->bSprinting = bIsSprinting;

    if (bIsSprinting)
    {
        GetWorld()->GetTimerManager().ClearTimer(StaminaPenaltyTimerHandle);
        GetWorld()->GetTimerManager().SetTimer(StaminaDrainTimerHandle, this, &UHamaComponent::DrainStamina, 0.1f, true);
    }
    else
    {
        if (OwnerCharacter)
        {
            if (OwnerCharacter->IsAimButtonHold())
            {
                SetAiming(true);
                OwnerCharacter->OnAim(true);
            }
            if (OwnerCharacter->IsFireButtonHolded())
            {
                OwnerCharacter->FireActionPressed();
            }
            if (OwnerCharacter->CurrentWeapon && OwnerCharacter->CurrentWeapon->CanReload())
            {
                OwnerCharacter->CurrentWeapon->Reload();
            }
        }

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

    if (OwnerCharacter->HasAuthority())
    {
        Stamina = FMath::Clamp(Stamina - StaminaDrainRate * 0.1f, 0.f, MaxStamina);
    }
}

void UHamaComponent::RegenerateStamina()
{
    if (bIsSlide) return;
    if (Stamina >= MaxStamina)
    {
        GetWorld()->GetTimerManager().ClearTimer(StaminaRegenTimerHandle);
        return;
    }

    if (OwnerCharacter->HasAuthority())
    {
        Stamina = FMath::Clamp(Stamina + StaminaRegenRate * 0.1f, 0.f, MaxStamina);
    }
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

    if (OwnerCharacter && OwnerCharacter->HasAuthority())
    {
        OwnerCharacter->ForceNetUpdate();
    }
}

void UHamaComponent::ResetStamina()
{
    MaxStamina = 100.f;
}

void UHamaComponent::SetDowned(bool NewValue)
{
    if (OwnerCharacter && OwnerCharacter->HasAuthority())
    {
        bIsDowned = NewValue;
        OwnerCharacter->ForceNetUpdate();
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