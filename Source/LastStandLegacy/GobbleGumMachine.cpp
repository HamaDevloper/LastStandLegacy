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
}

int32 AGobbleGumMachine::GetCurrentCost(const AHama* Player) const
{
    if (Player && Player->IsShoppingFreeActive())
    {
        return 0;
    }
    return BaseCost;
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
        return (InteractingPlayer == CurrentBuyer.Get());
    }

    return false;
}

bool AGobbleGumMachine::Client_PreInteract(AHama* InteractingPlayer)
{
    if (!InteractingPlayer || !CanInteract(InteractingPlayer)) return false;

    const float DistSq = FVector::DistSquared(GetActorLocation(), InteractingPlayer->GetActorLocation());
    if (DistSq > FMath::Square(InteractionDistance)) return false;

    // پشکنینی پارە تەنها کاتێک دەکرێت کە مەکینەکە لە دۆخی کڕین (Idle) بێت
    if (MachineState == EGobbleGumMachineState::Idle)
    {
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

    // کردار ١: کڕین لە مەکینەکە
    if (MachineState == EGobbleGumMachineState::Idle)
    {
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

    // کردار ٢: وەرگرتنی تۆپەکە پاش دەرچوون
    if (MachineState == EGobbleGumMachineState::Dispensed)
    {
        if (InteractingPlayer != CurrentBuyer.Get()) return;

        UGobbleGumComponent* GumComp = InteractingPlayer->GetGobbleGumComponent();
        if (GumComp && DispensedGum)
        {
            GumComp->Server_SetCurrentGum(DispensedGum);
        }

        GetWorldTimerManager().ClearTimer(TimerHandle_Timeout);
        ResetMachine();
    }
}

FString AGobbleGumMachine::GetInteractMessage(AHama* InteractingPlayer)
{
    if (!IsValid(InteractingPlayer)) return FString();

    if (MachineState == EGobbleGumMachineState::Idle)
    {
        const int32 Cost = GetCurrentCost(InteractingPlayer);
        if (Cost == 0)
        {
            return TEXT("Press F for GobbleGum [FREE]");
        }
        return FString::Printf(TEXT("Press F for GobbleGum [Cost %d]"), Cost);
    }

    if (MachineState == EGobbleGumMachineState::Dispensed)
    {
        if (InteractingPlayer == CurrentBuyer.Get())
        {
            const FString GumNameStr = DispensedGum ? DispensedGum->GumName.ToString() : TEXT("GobbleGum");
            return FString::Printf(TEXT("Press F to Take %s"), *GumNameStr);
        }

        // بۆ یاریزانانی تر دەق پیشان نادرێت چونکە مەکینەکە هی کەسێکی ترە
        return FString();
    }

    return FString();
}

void AGobbleGumMachine::StartRolling(AHama* Buyer)
{
    CurrentBuyer = Buyer;
    MachineState = EGobbleGumMachineState::Rolling;

    UGobbleGumComponent* GumComp = Buyer->GetGobbleGumComponent();
    if (GumComp)
    {
        DispensedGum = GumComp->RollNextGumFromPack();
    }

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
    CurrentBuyer.Reset();

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