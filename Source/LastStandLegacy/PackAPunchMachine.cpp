#include "PackAPunchMachine.h"
#include "Hama.h"
#include "HamaPlayerState.h"
#include "BaseWeapon.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/BoxComponent.h"

APackAPunchMachine::APackAPunchMachine()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    NetDormancy = DORM_DormantAll;

    SetNetUpdateFrequency(2.f);
    SetMinNetUpdateFrequency(1.f);

    MachineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MachineMesh"));
    RootComponent = MachineMesh;

    WeaponDisplaySocket = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponDisplaySocket"));
    WeaponDisplaySocket->SetupAttachment(RootComponent);

    DisplayWeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("DisplayWeaponMesh"));
    DisplayWeaponMesh->SetupAttachment(WeaponDisplaySocket);
    DisplayWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DisplayWeaponMesh->SetVisibility(false);

    InteractionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionVolume"));
    InteractionVolume->SetupAttachment(RootComponent);

    InteractionVolume->SetBoxExtent(FVector(45.f, 45.f, 45.f));
    InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionVolume->SetCollisionResponseToChannel(ECC_Intract, ECR_Block);
}

void APackAPunchMachine::BeginPlay()
{
    Super::BeginPlay();
}

void APackAPunchMachine::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (HasAuthority())
    {
        GetWorldTimerManager().ClearTimer(UpgradeTimerHandle);
        GetWorldTimerManager().ClearTimer(ExpirationTimerHandle);
    }

    Super::EndPlay(EndPlayReason);
}

void APackAPunchMachine::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Params;
    Params.bIsPushBased = true;
    Params.Condition = COND_None;

    DOREPLIFETIME_WITH_PARAMS_FAST(APackAPunchMachine, MachineState, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(APackAPunchMachine, RawWeaponClass, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(APackAPunchMachine, UpgradedWeaponClass, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(APackAPunchMachine, CurrentOwnerPlayer, Params);
}

// -----------------------------------------------------------------------------
// Interaction Interface & Network RPC
// -----------------------------------------------------------------------------

bool APackAPunchMachine::Client_PreInteract(AHama* InteractingPlayer)
{
    if (!IsValid(InteractingPlayer)) return false;

    if (InteractingPlayer->GetDeathMachine() || InteractingPlayer->IsDowned() || InteractingPlayer->IsDrinkingPerk()) return false;

    if (MachineState == EPaPState::Idle)
    {
        ABaseWeapon* Weapon = InteractingPlayer->GetCurrentWeapon();
        if (!Weapon || !Weapon->GetUpgradedWeaponClass())
        {
            return false;
        }

        AHamaPlayerState* PS = InteractingPlayer->GetPlayerState<AHamaPlayerState>();
        if (!PS || PS->GetPoints() < UpgradeCost)
        {
            if (RejectSound && InteractingPlayer->IsLocallyControlled())
            {
                UGameplayStatics::PlaySound2D(this, RejectSound);
            }
            return false;
        }
    }
    else if (MachineState == EPaPState::Upgrading)
    {
        return false;
    }
    else if (MachineState == EPaPState::ReadyForPickup)
    {
        if (CurrentOwnerPlayer != InteractingPlayer)
        {
            return false;
        }
    }

    return true;
}

bool APackAPunchMachine::CanInteract(AHama* InteractingPlayer)
{
    if (!IsValid(InteractingPlayer)) return false;
    if (InteractingPlayer->GetDeathMachine() || InteractingPlayer->IsDowned() || InteractingPlayer->IsDrinkingPerk()) return false;
    if (MachineState == EPaPState::Idle)
    {
        ABaseWeapon* Weapon = InteractingPlayer->GetCurrentWeapon();
        if (!Weapon) return false;

        return Weapon->GetUpgradedWeaponClass() != nullptr;
    }
    else if (MachineState == EPaPState::Upgrading)
    {
        return false;
    }
    else if (MachineState == EPaPState::ReadyForPickup)
    {
        return CurrentOwnerPlayer == InteractingPlayer;
    }

    return false;
}

void APackAPunchMachine::Interact(AHama* InteractingPlayer)
{
    if (!HasAuthority() || !IsValid(InteractingPlayer)) return;
    if (!CanInteract(InteractingPlayer)) return;

    if (MachineState == EPaPState::Idle)
    {
        AHamaPlayerState* PS = InteractingPlayer->GetPlayerState<AHamaPlayerState>();
        if (!PS || PS->GetPoints() < UpgradeCost)
        {
            return;
        }
    }

    if (HasAuthority())
    {
        ExecuteServerInteraction(InteractingPlayer);
    }
}


FString APackAPunchMachine::GetInteractMessage(AHama* InteractingPlayer)
{
    switch (MachineState)
    {
    case EPaPState::Idle:
        return FString::Printf(TEXT("Hold [E] Pack-A-Punch Weapon [%d Points]"), UpgradeCost);
    case EPaPState::Upgrading:
        return TEXT("Pack-A-Punching...");
    case EPaPState::ReadyForPickup:
        return TEXT("Hold [E] Take Upgraded Weapon");
    default:
        return TEXT("");
    }
}

// -----------------------------------------------------------------------------
// Server Logic
// -----------------------------------------------------------------------------

void APackAPunchMachine::ExecuteServerInteraction(AHama* InteractingPlayer)
{
    if (!HasAuthority() || !CanInteract(InteractingPlayer)) return;

    switch (MachineState)
    {
    case EPaPState::Idle:
        StartUpgradeProcess(InteractingPlayer);
        break;
    case EPaPState::ReadyForPickup:
        PickupUpgradedWeapon(InteractingPlayer);
        break;
    }
}

void APackAPunchMachine::StartUpgradeProcess(AHama* InteractingPlayer)
{
    AHamaPlayerState* PS = InteractingPlayer->GetPlayerState<AHamaPlayerState>();
    ABaseWeapon* CurrentWeapon = InteractingPlayer->GetCurrentWeapon();

    if (!PS || PS->GetPoints() < UpgradeCost || !CurrentWeapon) return;

    TSubclassOf<ABaseWeapon> NextClass = CurrentWeapon->GetUpgradedWeaponClass();
    if (!NextClass) return;

    PS->RemovePoints(UpgradeCost);

    CurrentOwnerPlayer = InteractingPlayer;
    RawWeaponClass = CurrentWeapon->GetClass();
    UpgradedWeaponClass = NextClass;
    MachineState = EPaPState::Upgrading;

    InteractingPlayer->SetCurrentlyUpgradingWeaponClass(CurrentWeapon->GetClass());
    InteractingPlayer->RemoveCurrentWeapon();

    FlushNetDormancy();
    ForceNetUpdate();

    MARK_PROPERTY_DIRTY_FROM_NAME(APackAPunchMachine, CurrentOwnerPlayer, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(APackAPunchMachine, RawWeaponClass, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(APackAPunchMachine, UpgradedWeaponClass, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(APackAPunchMachine, MachineState, this);

    UpdateVisuals();
    OnRep_PapMachineState();

    GetWorldTimerManager().SetTimer(
        UpgradeTimerHandle,
        this,
        &APackAPunchMachine::CompleteUpgradeProcess,
        UpgradeDuration,
        false
    );
}

void APackAPunchMachine::CompleteUpgradeProcess()
{
    if (!HasAuthority()) return;

    if (!IsValid(CurrentOwnerPlayer))
    {
        ResetMachineState();
        return;
    }

    MachineState = EPaPState::ReadyForPickup;

    FlushNetDormancy();
    ForceNetUpdate();

    MARK_PROPERTY_DIRTY_FROM_NAME(APackAPunchMachine, MachineState, this);

    UpdateVisuals();
    OnRep_PapMachineState();

    GetWorldTimerManager().SetTimer(
        ExpirationTimerHandle,
        this,
        &APackAPunchMachine::HandlePickupExpired,
        PickupExpirationTime,
        false
    );
}

void APackAPunchMachine::PickupUpgradedWeapon(AHama* InteractingPlayer)
{
    if (!HasAuthority() || !UpgradedWeaponClass || !IsValid(InteractingPlayer)) return;

    GetWorldTimerManager().ClearTimer(ExpirationTimerHandle);

    InteractingPlayer->GiveWeapon(UpgradedWeaponClass);

    ResetMachineState();
}

void APackAPunchMachine::HandlePickupExpired()
{
    if (!HasAuthority()) return;
    ResetMachineState();
}

void APackAPunchMachine::ResetMachineState()
{
    if (!HasAuthority()) return;

    GetWorldTimerManager().ClearTimer(UpgradeTimerHandle);
    GetWorldTimerManager().ClearTimer(ExpirationTimerHandle);

    if (IsValid(CurrentOwnerPlayer))
    {
        CurrentOwnerPlayer->SetCurrentlyUpgradingWeaponClass(nullptr);
    }

    MachineState = EPaPState::Idle;
    CurrentOwnerPlayer = nullptr;
    RawWeaponClass = nullptr;
    UpgradedWeaponClass = nullptr;

    FlushNetDormancy();
    ForceNetUpdate();

    MARK_PROPERTY_DIRTY_FROM_NAME(APackAPunchMachine, MachineState, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(APackAPunchMachine, CurrentOwnerPlayer, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(APackAPunchMachine, RawWeaponClass, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(APackAPunchMachine, UpgradedWeaponClass, this);

    UpdateVisuals();
    OnRep_PapMachineState();
}

// -----------------------------------------------------------------------------
// Replication Callbacks & Client Visuals
// -----------------------------------------------------------------------------

void APackAPunchMachine::OnRep_PapMachineState()
{
    UpdateVisuals();
    BP_OnPAPStateChanged(MachineState); 
}

void APackAPunchMachine::OnRep_UpgradedWeaponClass()
{
    UpdateVisuals();
}

void APackAPunchMachine::UpdateVisuals()
{
    if (IsRunningDedicatedServer()) return;

    TSubclassOf<ABaseWeapon> ActiveWeaponClassToShow = (MachineState == EPaPState::ReadyForPickup) ? UpgradedWeaponClass : RawWeaponClass;

    if (ActiveWeaponClassToShow)
    {
        if (const ABaseWeapon* DefaultWeapon = ActiveWeaponClassToShow->GetDefaultObject<ABaseWeapon>())
        {
            if (DefaultWeapon->WeaponMesh)
            {
                DisplayWeaponMesh->SetSkeletalMesh(DefaultWeapon->WeaponMesh->GetSkeletalMeshAsset());
            }
        }
    }
    else
    {
        DisplayWeaponMesh->SetSkeletalMesh(nullptr);
    }

    switch (MachineState)
    {
    case EPaPState::Idle:
        DisplayWeaponMesh->SetVisibility(false);
        break;
    case EPaPState::Upgrading:
    case EPaPState::ReadyForPickup:
        DisplayWeaponMesh->SetVisibility(true);
        break;
    }
}