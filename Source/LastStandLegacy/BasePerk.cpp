#include "BasePerk.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Hama.h"
#include "HamaPlayerState.h"
#include "LastStandLegacyGameState.h"
#include "Kismet/GameplayStatics.h"

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

bool ABasePerk::CanInteract(AHama* InteractingPlayer)
{
    if (!IsValid(InteractingPlayer)) return false;

    if (InteractingPlayer->IsDowned() ||
        InteractingPlayer->bIsDeathMachineActive ||
        InteractingPlayer->HasPerkID(PerkID) ||
        InteractingPlayer->IsDrinkingPerk())
    {
        return false;
    }

    ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>();
    if (!GS) return false;

    const bool bIsSoloQuickRevive = (PerkID == FName("QuickRevive") && GS->bIsSoloMatch);

    if (bIsSoloQuickRevive && SoloUsesLeftForQuickRevive <= 0)
    {
        return false;
    }

    return true;
}

bool ABasePerk::Client_PreInteract(AHama* InteractingPlayer)
{
    if (!CanInteract(InteractingPlayer)) return false;

    ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>();
    if (!GS) return false;

    const bool bIsSoloQuickRevive = (PerkID == FName("QuickRevive") && GS->bIsSoloMatch);

    if (!bIsSoloQuickRevive && !GS->bIsPowerOn)
    {
        if (RejectSound && InteractingPlayer->IsLocallyControlled())
        {
            UGameplayStatics::PlaySound2D(this, RejectSound);
        }
        return false;
    }

    AHamaPlayerState* PS = InteractingPlayer->GetPlayerState<AHamaPlayerState>();
    if (!PS || PS->GetPoints() < PerkCost)
    {
        if (RejectSound && InteractingPlayer->IsLocallyControlled())
        {
            UGameplayStatics::PlaySound2D(this, RejectSound);
        }
        return false;
    }

    return true;
}

void ABasePerk::Interact(AHama* InteractingPlayer)
{
    if (!HasAuthority() || !IsValid(InteractingPlayer) || !CanInteract(InteractingPlayer)) return;

    ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>();
    if (!GS) return;

    const bool bIsSoloQuickRevive = (PerkID == FName("QuickRevive") && GS->bIsSoloMatch);
    if (!bIsSoloQuickRevive && !GS->bIsPowerOn) return;

    AHamaPlayerState* PS = InteractingPlayer->GetPlayerState<AHamaPlayerState>();
    if (!PS || PS->GetPoints() < PerkCost) return;

    PS->RemovePoints(PerkCost);
    InteractingPlayer->Server_StartPerkDrink(this);

    if (bIsSoloQuickRevive)
    {
        SoloUsesLeftForQuickRevive--;
        if (SoloUsesLeftForQuickRevive <= 0)
        {
            SetActorHiddenInGame(true);
            SetActorEnableCollision(false);
        }
    }
}

FString ABasePerk::GetInteractMessage(AHama* InteractingPlayer)
{
    ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>();

    if (GS)
    {
        if (PerkID == FName("QuickRevive") && GS->bIsSoloMatch)
        {
            if (SoloUsesLeftForQuickRevive <= 0) return FString("");
            return FString::Printf(TEXT("Press F to buy %s [Cost: %d]"), *PerkID.ToString(), PerkCost);
        }

        if (!GS->bIsPowerOn)
        {
            return FString(TEXT("You must turn on the power first!"));
        }
    }

    return FString::Printf(TEXT("Press F to buy %s [Cost: %d]"), *PerkID.ToString(), PerkCost);
}