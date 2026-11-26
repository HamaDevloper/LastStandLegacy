#include "GambleMachine.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "HamaPlayerState.h"
#include "Hama.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"

AGambleMachine::AGambleMachine()
{
	PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    SetReplicatingMovement(false);
    SetNetDormancy(DORM_DormantAll);
    SetNetUpdateFrequency(1.f);
    SetMinNetUpdateFrequency(0.5f);

    GambleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GambleMesh"));
    RootComponent = GambleMesh;
}

#define LOCTEXT_NAMESPACE "GambleMachine"

void AGambleMachine::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearAllTimersForObject(this);
    }

    Super::EndPlay(EndPlayReason);
}

void AGambleMachine::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Params;
    Params.bIsPushBased = true;

    Params.Condition = COND_None;
    DOREPLIFETIME_WITH_PARAMS_FAST(AGambleMachine, MachineState, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AGambleMachine, CurrentGambler, Params);
}

bool AGambleMachine::CanInteract(AHama* InteractingPlayer)
{
    if (!InteractingPlayer || !InteractingPlayer->SetCanInteract()
        || MachineState == EGambleState::Gambling
        || MachineState == EGambleState::CoolDown)
    {
        return false;
    }

    if (MachineState == EGambleState::Ready && CurrentGambler != InteractingPlayer) return false;

    return true;
}

bool AGambleMachine::Client_PreInteract(AHama* InteractingPlayer)
{
    if (!IsValid(InteractingPlayer) || !CanInteract(InteractingPlayer)) return false;

    const float DisSq = FVector::DistSquared(GetActorLocation(), InteractingPlayer->GetActorLocation());
    if (DisSq > FMath::Square(250)) return false;

    if (MachineState == EGambleState::Idle)
    {
        AHamaPlayerState* PS = InteractingPlayer->GetPlayerState<AHamaPlayerState>();
        if (!PS || PS->GetPoints() < GamblePrice)
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

void AGambleMachine::Interact(AHama* InteractingPlayer)
{
    if (!IsValid(InteractingPlayer) || !CanInteract(InteractingPlayer)) return;

    const float DisSq = FVector::DistSquared(GetActorLocation(), InteractingPlayer->GetActorLocation());
    if (DisSq > FMath::Square(250)) return;

    if (MachineState == EGambleState::Idle)
    {
        AHamaPlayerState* PS = InteractingPlayer->GetPlayerState<AHamaPlayerState>();
        if (!PS || PS->GetPoints() < GamblePrice) return;

        PS->RemovePoints(GamblePrice);
        StartGamble(InteractingPlayer);
    }
    else if (MachineState == EGambleState::Ready)
    {
        GivePrize(InteractingPlayer);
    }
}

void AGambleMachine::StartGamble(AHama* Gambler)
{
    if (!HasAuthority()) return;

    UWorld* World = GetWorld();
    if (!World) return;

    MachineState = EGambleState::Gambling;
    CurrentGambler = Gambler;
    MARK_PROPERTY_DIRTY_FROM_NAME(AGambleMachine, MachineState, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AGambleMachine, CurrentGambler, this);

    FlushNetDormancy();
    ForceNetUpdate();

    if (Gambler->IsLocallyControlled())
    {
        OnRep_MachineState();
    }

    World->GetTimerManager().ClearTimer(StartGambleTimer);
    World->GetTimerManager().ClearTimer(EndGambleTimer);
    World->GetTimerManager().SetTimer(StartGambleTimer, this, &AGambleMachine::FinishGamble, GambleDuration, false);
}

void AGambleMachine::FinishGamble()
{
    if (!HasAuthority()) return;

    UWorld* World = GetWorld();
    if (!World)
    {
        ResetGambleMachine();
        return;
    }

    MachineState = EGambleState::Ready;
    MARK_PROPERTY_DIRTY_FROM_NAME(AGambleMachine, MachineState, this);

    FlushNetDormancy();
    ForceNetUpdate();

    World->GetTimerManager().ClearTimer(EndGambleTimer);
    World->GetTimerManager().SetTimer(EndGambleTimer, this, &AGambleMachine::ResetGambleMachine, ReadyDuration, false);

    if (GetNetMode() != NM_DedicatedServer)
    {
        OnRep_MachineState();
    }
}

void AGambleMachine::GivePrize(AHama* ClaimingPlayer)
{
    if (!HasAuthority() || !ClaimingPlayer) return;
    if (MachineState != EGambleState::Ready || CurrentGambler != ClaimingPlayer) return;

    AHamaPlayerState* PS = ClaimingPlayer->GetPlayerState<AHamaPlayerState>();
    if (!PS) return;

    const int32 Roll = FMath::RandRange(1, 100);
    int32 PrizePoints = 0;

    if (Roll <= 38)
    {
        // زەرەر: 400 - 600
        PrizePoints = FMath::RandRange(4, 6) * 100;
    }
    else if (Roll <= 68)
    {
        // نزیک یەکسان: 800 - 950
        PrizePoints = FMath::RandRange(16, 19) * 50;
    }
    else if (Roll <= 88)
    {
        // قازانجی کەم: 1200 - 1400
        PrizePoints = FMath::RandRange(12, 14) * 100;
    }
    else if (Roll <= 97)
    {
        // قازانجی باش: 1600 - 1900
        PrizePoints = FMath::RandRange(16, 19) * 100;
    }
    else
    {
        // جاکپۆت: 2500
        PrizePoints = 2500;
    }

    PS->AddPoints(PrizePoints);

    StartCooldown();
}

void AGambleMachine::StartCooldown()
{
    if (!HasAuthority()) return;

    UWorld* World = GetWorld();
    if (!World)
    {
        ResetGambleMachine();
        return;
    }

    MachineState = EGambleState::CoolDown;
    CurrentGambler = nullptr;
    MARK_PROPERTY_DIRTY_FROM_NAME(AGambleMachine, MachineState, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AGambleMachine, CurrentGambler, this);

    FlushNetDormancy();
    ForceNetUpdate();

    if (GetNetMode() != NM_DedicatedServer)
    {
        OnRep_MachineState();
    }

    World->GetTimerManager().ClearTimer(StartGambleTimer);
    World->GetTimerManager().ClearTimer(EndGambleTimer);
    World->GetTimerManager().SetTimer(EndGambleTimer, this, &AGambleMachine::ResetGambleMachine, CooldownDuration, false);
}

void AGambleMachine::ResetGambleMachine()
{
    if (!HasAuthority()) return;

    MachineState = EGambleState::Idle;
    CurrentGambler = nullptr;
    MARK_PROPERTY_DIRTY_FROM_NAME(AGambleMachine, MachineState, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AGambleMachine, CurrentGambler, this);

    FlushNetDormancy();
    ForceNetUpdate();

    UWorld* World = GetWorld();
    if (!World) return;
    World->GetTimerManager().ClearTimer(StartGambleTimer);
    World->GetTimerManager().ClearTimer(EndGambleTimer);

    if (GetNetMode() != NM_DedicatedServer)
    {
        OnRep_MachineState();
    }
}

FText AGambleMachine::GetInteractMessage(AHama* InteractingPlayer)
{
    if (!IsValid(InteractingPlayer))
    {
        return FText::GetEmpty();
    }

    switch (MachineState)
    {
    case EGambleState::Idle:
    {
        FFormatNamedArguments Args;
        Args.Add(TEXT("Cost"), GamblePrice);

        return FText::Format(LOCTEXT("GambleWithCost", "Press [F] to Gamble [Cost: {Cost}]"), Args);
    }

    case EGambleState::Gambling:
        return LOCTEXT("GambleInProgress", "Gambling...");

    case EGambleState::Ready:
        if (CurrentGambler == InteractingPlayer)
        {
            return LOCTEXT("GambleClaim", "Press [F] to Claim Prize");
        }
        return FText::GetEmpty();

    default:
        return FText::GetEmpty();
    }
}

void AGambleMachine::OnRep_MachineState()
{

}

#undef LOCTEXT_NAMESPACE