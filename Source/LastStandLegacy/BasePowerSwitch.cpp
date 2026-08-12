
#include "BasePowerSwitch.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "LastStandLegacyGameState.h"
#include "Kismet/GameplayStatics.h"

ABasePowerSwitch::ABasePowerSwitch()
{
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
    bReplicates = true;

    NetDormancy = DORM_DormantAll;

    SetNetUpdateFrequency(1.0f);
    SetMinNetUpdateFrequency(0.5f);

    SwitchFrameMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwitchFrameMesh"));
    SwitchFrameMesh->SetMobility(EComponentMobility::Static);
    SwitchFrameMesh->SetCollisionProfileName(TEXT("BlockAll"));
    SwitchFrameMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    SwitchFrameMesh->PrimaryComponentTick.bCanEverTick = false;
    RootComponent = SwitchFrameMesh;

    SwitchLeverMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwitchLeverMesh"));
    SwitchLeverMesh->SetupAttachment(SwitchFrameMesh);
    SwitchLeverMesh->SetCollisionProfileName(TEXT("NoCollision"));
    SwitchLeverMesh->SetMobility(EComponentMobility::Movable);
    SwitchLeverMesh->PrimaryComponentTick.bCanEverTick = false;
}

void ABasePowerSwitch::BeginPlay()
{
    Super::BeginPlay();
}

void ABasePowerSwitch::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Param;
    Param.bIsPushBased = true;
    Param.Condition = COND_None;
    DOREPLIFETIME_WITH_PARAMS_FAST(ABasePowerSwitch, bIsSwitchedOn, Param);
}

bool ABasePowerSwitch::CanInteract(AHama* InteractingPlayer)
{
    if (!IsValid(InteractingPlayer) || bIsSwitchedOn) return false;
    if (InteractingPlayer->IsDowned() || InteractingPlayer->bIsDeathMachineActive || InteractingPlayer->IsDrinkingPerk()) return false;

    return true;
}

bool ABasePowerSwitch::Client_PreInteract(AHama* InteractingPlayer)
{
    if (!CanInteract(InteractingPlayer))
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("Cannot interact with the power switch at this time."));
        return false;
    }

    return true;
}

void ABasePowerSwitch::Interact(AHama* InteractingPlayer)
{
    if (!HasAuthority() || !CanInteract(InteractingPlayer)) return;

    bIsSwitchedOn = true;
    MARK_PROPERTY_DIRTY_FROM_NAME(ABasePowerSwitch, bIsSwitchedOn, this);

    FlushNetDormancy();
    ForceNetUpdate();

    if (UWorld* World = GetWorld())
    {
        if (ALastStandLegacyGameState* GS = World->GetGameState<ALastStandLegacyGameState>())
        {
            GS->SetPowerState(true);
        }
    }
    if (!IsRunningDedicatedServer())
    {
        OnRep_IsSwitchedOn();
    }
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("SERVER: Power Switch Activated Successfully!"));
    }
}

FString ABasePowerSwitch::GetInteractMessage(AHama* InteractingPlayer)
{
    if (bIsSwitchedOn)
    {
        return FString(TEXT(""));
    }
    return FString(TEXT("Press [F] to Turn On Power"));
}

void ABasePowerSwitch::OnRep_IsSwitchedOn()
{
    if (bIsSwitchedOn)
    {
        if (SwitchLeverMesh)
        {
            SwitchLeverMesh->SetRelativeRotation(FRotator(-80.0f, 0.0f, 0.0f));
        }

        if (PowerOnSound)
        {
            UGameplayStatics::PlaySoundAtLocation(this, PowerOnSound, GetActorLocation());
        }

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, TEXT("CLIENT: Power Switch Lever Pulled! Visuals Updated."));
        }
    }
}