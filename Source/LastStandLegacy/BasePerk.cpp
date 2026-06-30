#include "BasePerk.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Hama.h"
#include "HamaPlayerState.h"

ABasePerk::ABasePerk()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));

    RootComponent = TriggerBox;

    TriggerBox->SetMobility(EComponentMobility::Static);
    TriggerBox->PrimaryComponentTick.bCanEverTick = false;

    PerkMachineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PerkMachineMesh"));
    PerkMachineMesh->SetupAttachment(RootComponent);
    PerkMachineMesh->SetMobility(EComponentMobility::Static);
    PerkMachineMesh->PrimaryComponentTick.bCanEverTick = false;

    PerkID = FName(TEXT("FastHands"));
    PerkCost = 3000;
    BottleMesh = nullptr;
}

void ABasePerk::BeginPlay()
{
    Super::BeginPlay();
}

void ABasePerk::Interact(AHama* HamaChar)
{
    if (!HamaChar || !HasAuthority()) return;

    if (HamaChar->HasPerkID(PerkID)) return;

    AHamaPlayerState* PS = HamaChar->GetPlayerState<AHamaPlayerState>();
    if (PS && PS->GetPoints() >= PerkCost)
    {
        PS->RemovePoints(PerkCost);
        HamaChar->Multicast_PlayDrinkPerkAnimation(this);
    }
}