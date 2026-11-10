#include "BaseDoor.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "HamaPlayerState.h"
#include "Hama.h"
#include "ZombieDirectorSubsystem.h"
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
    DoorMesh->SetCanEverAffectNavigation(false);
}

void ABaseDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Param;
    Param.bIsPushBased = true;
    Param.Condition = COND_None;

    DOREPLIFETIME_WITH_PARAMS_FAST(ABaseDoor, bIsDoorOpen, Param);
}

int32 ABaseDoor::GetCurrentCost(const AHama* Player) const
{
    if (Player && Player->IsShoppingFreeActive())
    {
        return 0;
    }
    return DoorPrice;
}

bool ABaseDoor::CanInteract(AHama* InteractingPlayer)
{
    if (!IsValid(InteractingPlayer) || bIsDoorOpen) return false;

    if (!InteractingPlayer->SetCanInteract())
    {
        return false;
    }

    return true;
}

bool ABaseDoor::Client_PreInteract(AHama* InteractingPlayer)
{
    if (!CanInteract(InteractingPlayer)) return false;

    const float DistSq = FVector::DistSquared(GetActorLocation(), InteractingPlayer->GetActorLocation());
    if (DistSq > FMath::Square(250.0f)) return false;

    const int32 Cost = GetCurrentCost(InteractingPlayer);
    AHamaPlayerState* PS = InteractingPlayer->GetPlayerState<AHamaPlayerState>();
    if (!PS || PS->GetPoints() < Cost)
    {
        if (RejectSound && InteractingPlayer->IsLocallyControlled())
        {
            UGameplayStatics::PlaySound2D(this, RejectSound);
        }
        return false;
    }

    return true;
}

void ABaseDoor::Interact(AHama* InteractingPlayer)
{
    if (!HasAuthority() || !CanInteract(InteractingPlayer)) return;

    const float DistSq = FVector::DistSquared(GetActorLocation(), InteractingPlayer->GetActorLocation());
    if (DistSq > FMath::Square(250.0f)) return;

    const int32 Cost = GetCurrentCost(InteractingPlayer);
    AHamaPlayerState* PS = InteractingPlayer->GetPlayerState<AHamaPlayerState>();
    if (PS && PS->GetPoints() >= Cost)
    {
        OpenDoor(InteractingPlayer, PS, Cost);
    }
}

FString ABaseDoor::GetInteractMessage(AHama* InteractingPlayer)
{
    if (!IsValid(InteractingPlayer)) return FString();

    const int32 Cost = GetCurrentCost(InteractingPlayer);
    if (Cost == 0)
    {
        return TEXT("Press F to Open Door [FREE]");
    }
    return FString::Printf(TEXT("Press F to Open Door [Cost %d]"), Cost);
}

void ABaseDoor::OpenDoor(AHama* InteractingPlayer, AHamaPlayerState* PS, int32 Cost) 
{
    if (!HasAuthority() || !PS || bIsDoorOpen) return;

    if (Cost > 0)
    {
        PS->RemovePoints(Cost);
    }

    bIsDoorOpen = true;
    MARK_PROPERTY_DIRTY_FROM_NAME(ABaseDoor, bIsDoorOpen, this);

    // چالاککردنی سپۆن پۆینتەکانی ئەو ژوورەی کە ئەم دەرگایە دەیکاتەوە
    if (UWorld* World = GetWorld())
    {
        if (UZombieDirectorSubsystem* Director = World->GetSubsystem<UZombieDirectorSubsystem>())
        {
            if (UnlockZoneID > 0)
            {
                Director->UnlockZone(UnlockZoneID);
            }
            if (SecondaryZoneID > 0)
            {
                Director->UnlockZone(SecondaryZoneID);
            }
        }
    }

    FlushNetDormancy();
    ForceNetUpdate();
    SetNetDormancy(DORM_DormantAll);

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