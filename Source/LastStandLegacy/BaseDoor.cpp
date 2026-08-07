#include "BaseDoor.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "HamaPlayerState.h"
#include "Hama.h"
#include "Kismet/GameplayStatics.h"

ABaseDoor::ABaseDoor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicatingMovement(false);

    NetDormancy = DORM_Initial;

    SetNetUpdateFrequency(1.f);
    SetMinNetUpdateFrequency(0.5f);

    DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
    RootComponent = DoorMesh;
    DoorMesh->SetMobility(EComponentMobility::Movable);
    DoorMesh->SetCollisionProfileName(TEXT("BlockAll"));
    DoorMesh->PrimaryComponentTick.bCanEverTick = false;
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

bool ABaseDoor::Client_PreInteract(AHama* Player)
{
    if (!IsValid(Player) || bIsDoorOpen) return false;
    AHamaPlayerState* PS = Player->GetPlayerState<AHamaPlayerState>();
    if (!PS || PS->GetPoints() < DoorPrice)
    {
        if (RejectSound && Player->IsLocallyControlled())
        {
            UGameplayStatics::PlaySound2D(this, RejectSound);
        }

        return false;
    }

    return true;
}

void ABaseDoor::Interact(AHama* Player)
{
    if (!HasAuthority() || !Player || bIsDoorOpen) return;

    AHamaPlayerState* PS = Player->GetPlayerState<AHamaPlayerState>();
    if (PS && PS->GetPoints() >= DoorPrice)
    {
        OpenDoor(Player, PS);
    }
}

FString ABaseDoor::GetInteractMessage(AHama* InteractingPlayer)
{
    return FString::Printf(TEXT("Press F to Open Door [Cost %d]"), DoorPrice );
}

void ABaseDoor::OpenDoor(AHama* Player, AHamaPlayerState* PS)
{
    check(HasAuthority());
    if (!PS || bIsDoorOpen) return;

    PS->RemovePoints(DoorPrice);

    bIsDoorOpen = true;
    MARK_PROPERTY_DIRTY_FROM_NAME(ABaseDoor, bIsDoorOpen, this);

    FlushNetDormancy();
    ForceNetUpdate();

    OnRep_OpenDoor();
}

void ABaseDoor::OnRep_OpenDoor()
{
    if (bIsDoorOpen)
    {
        DoorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

        if (DoorOpenSound)
        {
            UGameplayStatics::PlaySoundAtLocation(
                this,
                DoorOpenSound,
                GetActorLocation(),
                1.f,
                1.f
            );
        }
    }

    EventOnDoorOpened(bIsDoorOpen);
}