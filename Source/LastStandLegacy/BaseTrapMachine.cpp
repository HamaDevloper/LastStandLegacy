#include "BaseTrapMachine.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Hama.h"
#include "HamaPlayerState.h"
#include "Kismet/GameplayStatics.h"

ABaseTrapMachine::ABaseTrapMachine()
{
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;

    bReplicates = true;
    SetReplicateMovement(false);
    SetNetUpdateFrequency(10.0f);
    SetMinNetUpdateFrequency(2.0f);

    SetNetDormancy(DORM_DormantAll);

    RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComp"));
    SetRootComponent(RootComp);

    MachineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MachineMesh"));
    MachineMesh->SetupAttachment(RootComp);
    MachineMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

    InteractCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractCollision"));
    InteractCollision->SetupAttachment(RootComp);
    InteractCollision->SetSphereRadius(150.0f);
    InteractCollision->SetCollisionProfileName(TEXT("Trigger"));

    TrapState = ETrapMachineState::Ready;
    ActivationCost = 1000;
    ActiveDuration = 25.0f;
    CooldownDuration = 30.0f;
}

void ABaseTrapMachine::BeginPlay()
{
    Super::BeginPlay();
}

void ABaseTrapMachine::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams SharedParams;
    SharedParams.bIsPushBased = true;
    DOREPLIFETIME_WITH_PARAMS_FAST(ABaseTrapMachine, TrapState, SharedParams);
}

bool ABaseTrapMachine::CanInteract(AHama* InteractingPlayer)
{
    if (!IsValid(InteractingPlayer))  return false;
    if (!InteractingPlayer->SetCanInteract()) return false;
    return (TrapState == ETrapMachineState::Ready);
}

bool ABaseTrapMachine::Client_PreInteract(AHama* InteractingPlayer)
{
    if(!IsValid(InteractingPlayer) || !InteractingPlayer->SetCanInteract())
    {
        return false;
    }

    if (AHamaPlayerState* PS = InteractingPlayer->GetPlayerState<AHamaPlayerState>())
    {
        if (PS->GetPoints() < ActivationCost)
        {
            if (RejectSound && InteractingPlayer->IsLocallyControlled())
            {
                UGameplayStatics::PlaySound2D(this, RejectSound);
            }
            return false;
        }
    }

    float DistSq = FVector::DistSquared(GetActorLocation(), InteractingPlayer->GetActorLocation());
    if (DistSq > FMath::Square(250.0f))  return false;
 
    return true;
}

void ABaseTrapMachine::Interact(AHama* InteractingPlayer)
{
    if (!HasAuthority() || !CanInteract(InteractingPlayer))
    {
        return;
    }

    float DistSq = FVector::DistSquared(GetActorLocation(), InteractingPlayer->GetActorLocation());
    if(DistSq > FMath::Square(250.0f))  return;

    AHamaPlayerState* PS = InteractingPlayer->GetPlayerState<AHamaPlayerState>();
    if (PS && PS->GetPoints() < ActivationCost) return;

    ActivateTrap_Server();
}

void ABaseTrapMachine::SetTrapState_Server(ETrapMachineState NewState)
{
    if (!HasAuthority() || TrapState == NewState) return;

    TrapState = NewState;
    MARK_PROPERTY_DIRTY_FROM_NAME(ABaseTrapMachine, TrapState, this);

    if (TrapState == ETrapMachineState::Ready || TrapState == ETrapMachineState::Off)
    {
        FlushNetDormancy();
        SetNetDormancy(DORM_DormantAll);
    }
    else
    {
        SetNetDormancy(DORM_Awake);
        FlushNetDormancy();
    }

    OnRep_TrapState();
}

FString ABaseTrapMachine::GetInteractMessage(AHama* InteractingPlayer)
{
    switch (TrapState)
    {
    case ETrapMachineState::Ready:
        return FString::Printf(TEXT("Hold [F] to activate trap [Cost: %d]"), ActivationCost);
    case ETrapMachineState::Active:
        return TEXT("Trap is currently active...");
    case ETrapMachineState::CoolDown:
        return TEXT("Trap is cooling down...");
    default:
        return TEXT("");
    }
}

void ABaseTrapMachine::ActivateTrap_Server()
{
    if (!HasAuthority()) return;

    SetTrapState_Server(ETrapMachineState::Active);

    GetWorldTimerManager().SetTimer(
        TimerHandle_ActiveDuration,
        this,
        &ABaseTrapMachine::CooldownTrap_Server,
        ActiveDuration,
        false
    );

    OnTrapActivated_Server();
}

void ABaseTrapMachine::CooldownTrap_Server()
{
    if (!HasAuthority()) return;

    SetTrapState_Server(ETrapMachineState::CoolDown);

    GetWorldTimerManager().SetTimer(
        TimerHandle_CooldownDuration,
        this,
        &ABaseTrapMachine::ResetTrap_Server,
        CooldownDuration,
        false
    );

    OnTrapCooldown_Server();
}

void ABaseTrapMachine::ResetTrap_Server()
{
    if (!HasAuthority()) return;

    SetTrapState_Server(ETrapMachineState::Ready);
    OnTrapReset_Server();
}

void ABaseTrapMachine::OnRep_TrapState()
{
    HandleStateVisuals_Client(TrapState);
}

void ABaseTrapMachine::HandleStateVisuals_Client(ETrapMachineState NewState)
{
    // ئەمە بە بەتاڵی دەمێنێتەوە بۆ ئەوەی منداڵەکان لە خوارەوە ئۆڤەرڕایدی بکەن
}