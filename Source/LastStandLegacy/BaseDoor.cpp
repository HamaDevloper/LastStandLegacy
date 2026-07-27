#include "BaseDoor.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "HamaPlayerState.h"
#include "Hama.h"

ABaseDoor::ABaseDoor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicatingMovement(false); // ✅ Optimal for static interactive props

    NetDormancy = DORM_DormantAll;

    SetNetUpdateFrequency(1.f);
    SetMinNetUpdateFrequency(0.5f);

    DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
    RootComponent = DoorMesh;
    DoorMesh->SetMobility(EComponentMobility::Movable);
    DoorMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    DoorMesh->PrimaryComponentTick.bCanEverTick = false;

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DoorBox"));
    TriggerBox->SetupAttachment(DoorMesh);
    TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
    TriggerBox->PrimaryComponentTick.bCanEverTick = false;
}

void ABaseDoor::BeginPlay()
{
    Super::BeginPlay();
}

void ABaseDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Param;
    Param.bIsPushBased = true;
    Param.Condition = COND_None;

    DOREPLIFETIME_WITH_PARAMS_FAST(ABaseDoor, bIsDoorOpen, Param);
}

bool ABaseDoor::CanInteract(AHama* InteractingPlayer)
{
    return InteractingPlayer && !bIsDoorOpen;
}

void ABaseDoor::Interact(AHama* Player)
{
    if (!HasAuthority() || !Player || bIsDoorOpen) return;

    AHamaPlayerState* PS = Player->GetPlayerState<AHamaPlayerState>();

    if (PS && PS->GetPoints() >= DoorPrice)
    {
        OpenDoor(Player);
    }
}

FString ABaseDoor::GetInteractMessage()
{
    return FString::Printf(TEXT("Press F to Open Door [Cost %d]"), DoorPrice);
}

void ABaseDoor::OpenDoor(AHama* Player)
{
    if (!Player) return;

    AHamaPlayerState* PS = Player->GetPlayerState<AHamaPlayerState>();
    if (PS)
    {
        PS->RemovePoints(DoorPrice);
    }

    bIsDoorOpen = true;
    MARK_PROPERTY_DIRTY_FROM_NAME(ABaseDoor, bIsDoorOpen, this);
    FlushNetDormancy();

    OnRep_OpenDoor();
}

void ABaseDoor::OnRep_OpenDoor()
{
    if (bIsDoorOpen)
    {
        DoorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    EventOnDoorOpened(bIsDoorOpen);
}