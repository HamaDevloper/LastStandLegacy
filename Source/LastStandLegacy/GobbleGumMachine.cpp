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
    if (!InteractingPlayer->SetCanInteract()) return false;

    if (MachineState == EGobbleGumMachineState::Idle)
    {
        if (UGobbleGumComponent* GumComp = InteractingPlayer->GetGobbleGumComponent())
        {
            return GumComp->CanBuyFromMachineThisRound();
        }
        return false;
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

#define LOCTEXT_NAMESPACE "GobbleGumMachine"

FText AGobbleGumMachine::GetInteractMessage(AHama* InteractingPlayer)
{
    if (!IsValid(InteractingPlayer))
    {
        return FText::GetEmpty();
    }

    // دۆخی ١: کڕینی گۆبڵگەم (Idle)
    if (MachineState == EGobbleGumMachineState::Idle)
    {

        if (UGobbleGumComponent* GumComp = InteractingPlayer->GetGobbleGumComponent())
        {
            if (!GumComp->CanBuyFromMachineThisRound())
            {
                return LOCTEXT("GobbleGumLimitReached", "Come back next round for another GobbleGum");
            }
        }

        const int32 Cost = GetCurrentCost(InteractingPlayer);

        if (Cost <= 0)
        {
            return LOCTEXT("GobbleGumBuyFree", "Press [F] for GobbleGum [FREE]");
        }

        FFormatNamedArguments Args;
        // هێشتنەوەی ژمارەکە بە ئینگلیزی (Latin Digits) لە هەموو زمانەکاندا
        Args.Add(TEXT("Cost"), FText::FromString(FString::FromInt(Cost)));

        return FText::Format(LOCTEXT("GobbleGumBuyWithCost", "Press [F] for GobbleGum [Cost: {Cost}]"), Args);
    }

    // دۆخی ٢: دەرچوونی تۆپەکە و هەڵگرتنەوەی لەلایەن کڕیارەوە (Dispensed)
    if (MachineState == EGobbleGumMachineState::Dispensed)
    {
        if (InteractingPlayer == CurrentBuyer.Get())
        {
            FFormatNamedArguments Args;

            // ئەگەر لەناو DataAssetـدا GumName لە جۆری FName بوو FText::FromName بەکاربهێنە، 
            // ئەگەریش لە بنەڕەتەوە FText بوو، دەتوانیت ڕاستەوخۆ DispensedGum->GumName بنووسیت
            const FText GumText = DispensedGum
                ? FText::FromString(DispensedGum->GumName.ToString())
                : LOCTEXT("DefaultGumName", "GobbleGum");

            Args.Add(TEXT("GumName"), GumText);

            return FText::Format(LOCTEXT("GobbleGumTake", "Press [F] to Take {GumName}"), Args);
        }

        // بۆ یاریزانانی تر هیچ پیشان نادرێت چونکە مەکینەکە بۆ کڕیارەکە قفڵ کراوە
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
        GumComp->SetHasBoughtThisRound(true);
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