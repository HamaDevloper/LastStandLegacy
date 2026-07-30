#include "PackAPunchMachine.h"
#include "Hama.h"
#include "HamaPlayerState.h"
#include "BaseWeapon.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

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
}

void APackAPunchMachine::BeginPlay()
{
    Super::BeginPlay();
}

void APackAPunchMachine::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    if (HasAuthority())
    {
        GetWorldTimerManager().ClearTimer(UpgradeTimerHandle);
        GetWorldTimerManager().ClearTimer(ExpirationTimerHandle);
    }
}

void APackAPunchMachine::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Params;
    Params.bIsPushBased = true;
    Params.Condition = COND_None;

    DOREPLIFETIME_WITH_PARAMS_FAST(APackAPunchMachine, MachineState, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(APackAPunchMachine, UpgradedWeaponClass, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(APackAPunchMachine, CurrentOwnerPlayer, Params);
}

// -----------------------------------------------------------------------------
// Interaction Interface
// -----------------------------------------------------------------------------

bool APackAPunchMachine::CanInteract(AHama* InteractingPlayer)
{
    if (!IsValid(InteractingPlayer)) return false;
    if (InteractingPlayer->GetDeathMachine()) return false;

    if (MachineState == EPaPState::Idle)
    {
        ABaseWeapon* Weapon = InteractingPlayer->GetCurrentWeapon();
        if (!Weapon) return false;

        return Weapon->GetUpgradedWeaponClass() != nullptr;
    }
    else if (MachineState == EPaPState::ReadyForPickup)
    {
        return CurrentOwnerPlayer == InteractingPlayer;
    }

    return false;
}

void APackAPunchMachine::Interact(AHama* Player)
{
    if (!IsValid(Player)) return;

    if (MachineState == EPaPState::Idle)
    {
        AHamaPlayerState* PS = Player->GetPlayerState<AHamaPlayerState>();

        if (!PS || PS->GetPoints() < UpgradeCost)
        {
            if (RejectSound)
            {
                UGameplayStatics::PlaySound2D(GetWorld(), RejectSound);
            }
            return;
        }
    }

    if (HasAuthority())
    {
        ExecuteServerInteraction(Player);
    }
}

FString APackAPunchMachine::GetInteractMessage()
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
// Server Interaction Logic
// -----------------------------------------------------------------------------

void APackAPunchMachine::ExecuteServerInteraction(AHama* InteractingPlayer)
{
    // Fix B: Strict Anti-Cheat & Validation Check on Server
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
    UpgradedWeaponClass = NextClass;
    MachineState = EPaPState::Upgrading;

    InteractingPlayer->RemoveCurrentWeapon();

    FlushNetDormancy();
    MARK_PROPERTY_DIRTY_FROM_NAME(APackAPunchMachine, CurrentOwnerPlayer, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(APackAPunchMachine, UpgradedWeaponClass, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(APackAPunchMachine, MachineState, this);

    UpdateVisuals();

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
    MARK_PROPERTY_DIRTY_FROM_NAME(APackAPunchMachine, MachineState, this);

    UpdateVisuals();

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

    MachineState = EPaPState::Idle;
    CurrentOwnerPlayer = nullptr;
    UpgradedWeaponClass = nullptr;

    FlushNetDormancy();
    MARK_PROPERTY_DIRTY_FROM_NAME(APackAPunchMachine, MachineState, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(APackAPunchMachine, CurrentOwnerPlayer, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(APackAPunchMachine, UpgradedWeaponClass, this);

    UpdateVisuals();
}

void APackAPunchMachine::OnRep_PapMachineState()
{
    UpdateVisuals();
}

// Fix A: Added OnRep for UpgradedWeaponClass to fix replication race condition
void APackAPunchMachine::OnRep_UpgradedWeaponClass()
{
    UpdateVisuals();
}

void APackAPunchMachine::UpdateVisuals()
{
    if (UpgradedWeaponClass)
    {
        if (ABaseWeapon* DefaultWeapon = UpgradedWeaponClass->GetDefaultObject<ABaseWeapon>())
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