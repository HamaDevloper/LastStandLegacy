#include "BasePowerSwitch.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "LastStandLegacyGameState.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "PowerSwitchSystem"

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

void ABasePowerSwitch::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Param;
    Param.bIsPushBased = true;
    Param.Condition = COND_None;
    Param.RepNotifyCondition = REPNOTIFY_OnChanged;

    DOREPLIFETIME_WITH_PARAMS_FAST(ABasePowerSwitch, bIsSwitchedOn, Param);
}

bool ABasePowerSwitch::CanInteract(AHama* InteractingPlayer)
{
    if (!IsValid(InteractingPlayer) || bIsSwitchedOn) return false;
    if (!InteractingPlayer->SetCanInteract()) return false;

    return true;
}

bool ABasePowerSwitch::Client_PreInteract(AHama* InteractingPlayer)
{
    if (!InteractingPlayer || !CanInteract(InteractingPlayer))
    {
        return false;
    }

    constexpr float MaxDistSq = 250.0f * 250.0f;
    const float DistSq = FVector::DistSquared(GetActorLocation(), InteractingPlayer->GetActorLocation());
    if (DistSq > MaxDistSq)
    {
        return false;
    }

    return true;
}

void ABasePowerSwitch::Interact(AHama* InteractingPlayer)
{
    if (!InteractingPlayer || !HasAuthority() || !CanInteract(InteractingPlayer)) return;

    constexpr float MaxDistSq = 250.0f * 250.0f;
    const float DistSq = FVector::DistSquared(GetActorLocation(), InteractingPlayer->GetActorLocation());
    if (DistSq > MaxDistSq) return;

    bIsSwitchedOn = true;
    MARK_PROPERTY_DIRTY_FROM_NAME(ABasePowerSwitch, bIsSwitchedOn, this);

    // نوێکردنەوەی دۆخی کارەبا لەسەر ئاستی سێرڤەر لەناو GameState
    if (UWorld* World = GetWorld())
    {
        if (ALastStandLegacyGameState* GS = World->GetGameState<ALastStandLegacyGameState>())
        {
            GS->SetPowerState(true);
        }
    }

    // بەئاگاهێنانەوەی کاتی بۆ ناردنی پاکێتی تۆڕ بە کڕایەنتەکان
    FlushNetDormancy();
    ForceNetUpdate();

    // خستنەوەی دەستبەجێی ئەکتەرەکە بۆ ناو DORM_DormantAll بۆ ڕێگریکردن لە بەفیڕۆچوونی توانای سێرڤەر
    SetNetDormancy(DORM_DormantAll);

    // ئەگەر Listen-Server یان Standalone بوو، ئەرکی جوڵاندنی دەسکەکە ڕاستەوخۆ جێبەجێ دەکرێت
    if (!IsRunningDedicatedServer())
    {
        OnRep_IsSwitchedOn();
    }
}

FText ABasePowerSwitch::GetInteractMessage(AHama* InteractingPlayer)
{
    if (bIsSwitchedOn)
    {
        return FText::GetEmpty();
    }

    return LOCTEXT("PowerSwitchTurnOn", "Press [F] to Turn On Power");
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

        // ناچالاککردنی کۆلیژنی پشکنین بۆ ئەوەی هیچ Traceێکی تر بەر ئەم سویچە نەکەوێت
        if (SwitchFrameMesh)
        {
            SwitchFrameMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
        }
    }
}

#undef LOCTEXT_NAMESPACE