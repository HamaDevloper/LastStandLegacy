#include "GobbleGumMachine.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "HamaPlayerState.h"
#include "Hama.h"
#include "GobbleGumComponent.h"
#include "GobbleGumData.h"
#include "Kismet/GameplayStatics.h"

AGobbleGumMachine::AGobbleGumMachine()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicatingMovement(false);

    NetDormancy = DORM_Initial;

    SetNetUpdateFrequency(1.0f);
    SetMinNetUpdateFrequency(0.5f);

    MachineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MachineMesh"));
    RootComponent = MachineMesh;
    MachineMesh->SetMobility(EComponentMobility::Static);
    MachineMesh->SetCollisionProfileName(TEXT("BlockAll"));
    MachineMesh->PrimaryComponentTick.bCanEverTick = false;

    GumBallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GumBallMesh"));
    GumBallMesh->SetupAttachment(RootComponent);
    GumBallMesh->SetMobility(EComponentMobility::Movable);
    GumBallMesh->SetVisibility(false);
    GumBallMesh->SetCollisionProfileName(TEXT("NoCollision"));
    GumBallMesh->PrimaryComponentTick.bCanEverTick = false;
}

void AGobbleGumMachine::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Param;
    Param.bIsPushBased = true;
    Param.Condition = COND_None;

    DOREPLIFETIME_WITH_PARAMS_FAST(AGobbleGumMachine, MachineState, Param);
    DOREPLIFETIME_WITH_PARAMS_FAST(AGobbleGumMachine, DispensedGum, Param);
    DOREPLIFETIME_WITH_PARAMS_FAST(AGobbleGumMachine, CurrentBuyer, Param);
}

bool AGobbleGumMachine::HasPlayerReachedRoundLimit(const AHama* Player) const
{
    if (!IsValid(Player)) return true;

    if (const UGobbleGumComponent* GumComp = Player->GetGobbleGumComponent())
    {
        return GumComp->GetPurchasesThisRound() >= MaxPurchasesPerRound;
    }

    return true;
}

int32 AGobbleGumMachine::GetCurrentCost(const AHama* Player) const
{
    if (Player && Player->IsShoppingFreeActive())
    {
        return 0;
    }

    if (!IsValid(Player))
    {
        return BaseCost;
    }

    const UGobbleGumComponent* GumComp = Player->GetGobbleGumComponent();
    if (!GumComp)
    {
        return BaseCost;
    }

    const uint8 Purchases = GumComp->GetPurchasesThisRound();
    if (Purchases == 0)
    {
        return BaseCost;
    }

    // لێکدانی نرخ بە شێوەی BaseCost * (CostMultiplier ^ Purchases)
    const double MultiplierFactor = FMath::Pow(static_cast<double>(CostMultiplier), static_cast<double>(Purchases));
    const int64 ScaledCost = FMath::RoundToInt64(static_cast<double>(BaseCost) * MultiplierFactor);

    // سنووردارکردن بۆ ڕێگری لە تێپەڕبوونی سنووری int32
    return static_cast<int32>(FMath::Clamp<int64>(ScaledCost, 0, 100000000));
}

bool AGobbleGumMachine::CanInteract(AHama* InteractingPlayer)
{
    if (!IsValid(InteractingPlayer)) return false;

    if (!InteractingPlayer->SetCanInteract())
    {
        return false;
    }

    if (MachineState == EGobbleGumMachineState::Idle)
    {
        return true;
    }

    if (MachineState == EGobbleGumMachineState::Dispensed)
    {
        return (InteractingPlayer == CurrentBuyer);
    }

    return false;
}

bool AGobbleGumMachine::Client_PreInteract(AHama* InteractingPlayer)
{
    if (!InteractingPlayer || !CanInteract(InteractingPlayer)) return false;

    const float DistSq = FVector::DistSquared(GetActorLocation(), InteractingPlayer->GetActorLocation());
    if (DistSq > FMath::Square(InteractionDistance)) return false;

    if (MachineState == EGobbleGumMachineState::Idle)
    {
        if (HasPlayerReachedRoundLimit(InteractingPlayer))
        {
            if (RejectSound && InteractingPlayer->IsLocallyControlled())
            {
                UGameplayStatics::PlaySound2D(this, RejectSound);
            }
            return false;
        }

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
    }

    return true;
}

void AGobbleGumMachine::Interact(AHama* InteractingPlayer)
{
    if (!HasAuthority() || !InteractingPlayer || !CanInteract(InteractingPlayer)) return;

    const float DistSq = FVector::DistSquared(GetActorLocation(), InteractingPlayer->GetActorLocation());
    if (DistSq > FMath::Square(InteractionDistance)) return;

    if (MachineState == EGobbleGumMachineState::Idle)
    {
        if (HasPlayerReachedRoundLimit(InteractingPlayer))
        {
            return;
        }

        const int32 Cost = GetCurrentCost(InteractingPlayer);
        AHamaPlayerState* PS = InteractingPlayer->GetPlayerState<AHamaPlayerState>();
        if (PS && PS->GetPoints() >= Cost)
        {
            if (Cost > 0)
            {
                PS->RemovePoints(Cost);
            }

            StartRolling(InteractingPlayer);
        }
        return;
    }

    if (MachineState == EGobbleGumMachineState::Dispensed)
    {
        if (InteractingPlayer != CurrentBuyer) return;

        UGobbleGumComponent* GumComp = InteractingPlayer->GetGobbleGumComponent();
        if (GumComp && DispensedGum)
        {
            GumComp->Server_SetCurrentGum(DispensedGum);
        }

        GetWorldTimerManager().ClearTimer(TimerHandle_Timeout);
        ResetMachine();
    }
}

#define LOCTEXT_NAMESPACE "GobbleGumMachine"

FText AGobbleGumMachine::GetInteractMessage(AHama* InteractingPlayer)
{
    if (!IsValid(InteractingPlayer))
    {
        return FText::GetEmpty();
    }

    if (MachineState == EGobbleGumMachineState::Idle)
    {
        // ١. ئەگەر هەموو هەلەکانی ئەم خولەی بەکارهێنابوو، نامەی چاوەڕوانی پیشان بدە
        if (HasPlayerReachedRoundLimit(InteractingPlayer))
        {
            return LOCTEXT("GobbleGumWaitNextRound", "Come back next round for another GobbleGum");
        }

        // ٢. پیشاندانی نرخە نوێیەکە (کە لەگەڵ هەر کڕینێکدا بەرز بووەتەوە)
        const int32 Cost = GetCurrentCost(InteractingPlayer);

        if (Cost <= 0)
        {
            return LOCTEXT("GobbleGumBuyFree", "Press [F] for GobbleGum [FREE]");
        }

        FFormatNamedArguments Args;
        Args.Add(TEXT("Cost"), FText::FromString(FString::FromInt(Cost)));

        return FText::Format(LOCTEXT("GobbleGumBuyWithCost", "Press [F] for GobbleGum [Cost: {Cost}]"), Args);
    }

    if (MachineState == EGobbleGumMachineState::Dispensed)
    {
        if (InteractingPlayer == CurrentBuyer)
        {
            FFormatNamedArguments Args;
            const FText GumText = DispensedGum
                ? DispensedGum->GumName
                : LOCTEXT("DefaultGumName", "GobbleGum");

            Args.Add(TEXT("GumName"), GumText);

            return FText::Format(LOCTEXT("GobbleGumTake", "Press [F] to Take {GumName}"), Args);
        }

        return FText::GetEmpty();
    }

    return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE

void AGobbleGumMachine::StartRolling(AHama* Buyer)
{
    CurrentBuyer = Buyer;
    MachineState = EGobbleGumMachineState::Rolling;

    UGobbleGumComponent* GumComp = Buyer->GetGobbleGumComponent();
    if (GumComp)
    {
        GumComp->IncrementPurchasesThisRound();
        DispensedGum = GumComp->RollNextGumFromPack();
    }

    MARK_PROPERTY_DIRTY_FROM_NAME(AGobbleGumMachine, CurrentBuyer, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AGobbleGumMachine, MachineState, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AGobbleGumMachine, DispensedGum, this);

    FlushNetDormancy();
    ForceNetUpdate();

    HandleVisualStateChanged(MachineState);

    GetWorldTimerManager().SetTimer(
        TimerHandle_Roll,
        this,
        &AGobbleGumMachine::DispenseGum,
        RollDuration,
        false
    );
}

void AGobbleGumMachine::DispenseGum()
{
    MachineState = EGobbleGumMachineState::Dispensed;

    MARK_PROPERTY_DIRTY_FROM_NAME(AGobbleGumMachine, MachineState, this);
    FlushNetDormancy();
    ForceNetUpdate();

    HandleVisualStateChanged(MachineState);

    GetWorldTimerManager().SetTimer(
        TimerHandle_Timeout,
        this,
        &AGobbleGumMachine::ResetMachine,
        PickupTimeout,
        false
    );
}

void AGobbleGumMachine::ResetMachine()
{
    MachineState = EGobbleGumMachineState::Idle;
    DispensedGum = nullptr;
    CurrentBuyer = nullptr;

    MARK_PROPERTY_DIRTY_FROM_NAME(AGobbleGumMachine, CurrentBuyer, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AGobbleGumMachine, MachineState, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AGobbleGumMachine, DispensedGum, this);

    FlushNetDormancy();
    ForceNetUpdate();
    SetNetDormancy(DORM_DormantAll);

    HandleVisualStateChanged(MachineState);
}

void AGobbleGumMachine::OnRep_MachineState()
{
    HandleVisualStateChanged(MachineState);
}

void AGobbleGumMachine::OnRep_DispensedGum()
{
}

void AGobbleGumMachine::HandleVisualStateChanged(EGobbleGumMachineState NewState)
{
    switch (NewState)
    {
    case EGobbleGumMachineState::Idle:
        if (GumBallMesh)
        {
            GumBallMesh->SetVisibility(false);
        }
        break;

    case EGobbleGumMachineState::Rolling:
        if (GumBallMesh)
        {
            GumBallMesh->SetVisibility(false);
        }
        if (RollSound)
        {
            UGameplayStatics::PlaySoundAtLocation(this, RollSound, GetActorLocation());
        }
        break;

    case EGobbleGumMachineState::Dispensed:
        if (GumBallMesh)
        {
            GumBallMesh->SetVisibility(true);
        }
        if (DispenseSound)
        {
            UGameplayStatics::PlaySoundAtLocation(this, DispenseSound, GetActorLocation());
        }
        break;

    case EGobbleGumMachineState::Cooldown:
        break;
    }
}