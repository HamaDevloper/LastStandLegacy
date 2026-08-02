#include "BasePerk.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Hama.h"
#include "HamaPlayerState.h"
#include "LastStandLegacyGameState.h"

ABasePerk::ABasePerk()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicatingMovement(false);

    NetDormancy = DORM_Initial;
    SetNetUpdateFrequency(1.0f);
    SetMinNetUpdateFrequency(0.5f);

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;
    TriggerBox->SetMobility(EComponentMobility::Static);
    TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
    TriggerBox->SetCollisionResponseToChannel(ECC_Intract, ECR_Block);
    TriggerBox->PrimaryComponentTick.bCanEverTick = false;

    PerkMachineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PerkMachineMesh"));
    PerkMachineMesh->SetupAttachment(RootComponent);
    PerkMachineMesh->SetMobility(EComponentMobility::Static);
    PerkMachineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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

    ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>();
    if (!GS) return;

    bool bCanBuy = false;

    if (PerkID == FName("QuickRevive") && GS->bIsSoloMatch)
    {
        if (SoloUsesLeftForQuickRevive <= 0) return;
        bCanBuy = true;
    }
    else
    {
        if (!GS->bIsPowerOn) return;
        bCanBuy = true;
    }

    if (!bCanBuy) return;
    if (HamaChar->HasPerkID(PerkID)) return;
    if (HamaChar->DrinkingPerkTimer()) return;

    AHamaPlayerState* PS = HamaChar->GetPlayerState<AHamaPlayerState>();
    if (PS && PS->GetPoints() >= PerkCost)
    {
        PS->RemovePoints(PerkCost);

        HamaChar->Server_StartPerkDrink(this);

        if (PerkID == FName("QuickRevive") && GS->bIsSoloMatch)
        {
            SoloUsesLeftForQuickRevive--;
            if (SoloUsesLeftForQuickRevive <= 0)
            {
                SetActorHiddenInGame(true);
                SetActorEnableCollision(false);
            }
        }
    }
}

FString ABasePerk::GetInteractMessage()
{
   
    ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>();

    if (GS)
    {
        if (PerkID == FName("QuickRevive") && GS->bIsSoloMatch)
        {
            if (SoloUsesLeftForQuickRevive <= 0) return FString("");
            return FString::Printf(TEXT("Press F to buy %s [Cost: %d]"), *PerkID.ToString(), PerkCost);
        }

        // مەرجی کارەبا
        if (!GS->bIsPowerOn)
        {
            return FString(TEXT("You must turn on the power first!"));
        }
    }

    return FString::Printf(TEXT("Press F to buy %s [Cost: %d]"), *PerkID.ToString(), PerkCost);
}

bool ABasePerk::CanInteract(AHama* InteractingPlayer)
{
    if (!InteractingPlayer) return false;

    if (InteractingPlayer->IsDowned() || InteractingPlayer->bIsDeathMachineActive)
    {
        return false;
    }

    if (InteractingPlayer->HasPerkID(PerkID))
    {
        return false;
    }

    ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>();
    if (GS && PerkID == FName("QuickRevive") && GS->bIsSoloMatch)
    {
        if (SoloUsesLeftForQuickRevive <= 0)
        {
            return false;
        }
    }

    return true;
}