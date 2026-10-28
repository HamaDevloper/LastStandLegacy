// Fill out your copyright notice in the Description page of Project Settings.

#include "BasePerk.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Hama.h"
#include "HamaPlayerState.h"
#include "LastStandLegacyGameState.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"

ABasePerk::ABasePerk()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicatingMovement(false);

    // ئەکتەرەکە بە تەواوی دەخەوێت تا کاتێک گۆڕانکاری ڕوودەدات
    NetDormancy = DORM_DormantAll;
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
    PerkMachineMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    PerkMachineMesh->SetCollisionResponseToChannel(ECC_Intract, ECR_Ignore);
    PerkMachineMesh->PrimaryComponentTick.bCanEverTick = false;

    MachineLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MachineLight"));
    MachineLight->SetupAttachment(PerkMachineMesh);
    MachineLight->SetIntensity(2500.f);
    MachineLight->SetLightColor(FLinearColor(0.2f, 0.7f, 1.0f));
    MachineLight->PrimaryComponentTick.bCanEverTick = false;

    PerkID = FName(TEXT("FastHands"));
    PerkCost = 3000;
    BottleMesh = nullptr;
}

void ABasePerk::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams PushParams;
    PushParams.bIsPushBased = true;
    PushParams.RepNotifyCondition = REPNOTIFY_OnChanged;

    DOREPLIFETIME_WITH_PARAMS_FAST(ABasePerk, bIsDepleted, PushParams);
}

bool ABasePerk::CanInteract(AHama* InteractingPlayer)
{
    if (bIsDepleted) return false;
    if (!IsValid(InteractingPlayer)) return false;
    if (!InteractingPlayer->SetCanInteract()) return false;
    if (InteractingPlayer->HasPerkID(PerkID)) return false;

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

    const float DistSq = FVector::DistSquared(GetActorLocation(), InteractingPlayer->GetActorLocation());
    if (DistSq > MaxInteractDistanceSq) return false;

    return true;
}

void ABasePerk::Interact(AHama* InteractingPlayer)
{
    if (!HasAuthority() || !IsValid(InteractingPlayer) || !CanInteract(InteractingPlayer)) return;

    ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>();
    if (!GS) return;

    const float DistSq = FVector::DistSquared(GetActorLocation(), InteractingPlayer->GetActorLocation());
    if (DistSq > MaxInteractDistanceSq) return;

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
            bIsDepleted = true;
            MARK_PROPERTY_DIRTY_FROM_NAME(ABasePerk, bIsDepleted, this);

            OnRep_IsDepleted();
            FlushNetDormancy();
        }
    }
}

void ABasePerk::OnRep_IsDepleted()
{
    // ١. ناچالاککردنی کارلێککردن
    if (TriggerBox)
    {
        TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // ٢. کوژاندنەوەی گڵۆپ
    if (MachineLight)
    {
        MachineLight->SetVisibility(false);
    }

    // ٣. کوژاندنەوەی Emissiveی مەتریالەکە لەسەر ئامێرەکە
    if (PerkMachineMesh)
    {
        PerkMachineMesh->SetScalarParameterValueOnMaterials(TEXT("EmissiveIntensity"), 0.0f);
    }

    // ٤. دەنگی شۆرت و کوژانەوەی کارەبا
    if (PowerDownSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, PowerDownSound, GetActorLocation());
    }
}

FString ABasePerk::GetInteractMessage(AHama* InteractingPlayer)
{
    if (bIsDepleted) return FString("");

    ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>();
    if (GS)
    {
        const bool bIsSoloQuickRevive = (PerkID == FName("QuickRevive") && GS->bIsSoloMatch);
        if (bIsSoloQuickRevive)
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