#include "HamaPlayerController.h"
#include "HamaMainWidget.h"
#include "HamaPlayerState.h"
#include "Hama.h"
#include "Blueprint/UserWidget.h"
#include "LastStandLegacyGameState.h"

void AHamaPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (IsLocalController())
    {
        SetupClientUI();
    }
}

void AHamaPlayerController::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();

    if (IsLocalController())
    {
        SetupClientUI();
    }
}

void AHamaPlayerController::AcknowledgePossession(APawn* P)
{
    Super::AcknowledgePossession(P);

    if (IsLocalController())
    {
        SetupClientUI();
    }
}

void AHamaPlayerController::SetupClientUI()
{
    if (!MainWidgetRef && MainWidgetClass)
    {
        MainWidgetRef = CreateWidget<UHamaMainWidget>(this, MainWidgetClass);
        if (MainWidgetRef)
        {
            MainWidgetRef->AddToViewport();
        }
    }

    if (!MainWidgetRef) return;

    if (AHamaPlayerState* HamaPS = GetPlayerState<AHamaPlayerState>())
    {
        MainWidgetRef->HandlePointsUpdate(HamaPS->GetPoints());
        MainWidgetRef->HandleKillsUpdate(HamaPS->GetKills());
    }

    if (AHama* HamaCharacter = Cast<AHama>(GetPawn()))
    {
        MainWidgetRef->InitializeWidget(HamaCharacter);
    }

    if (ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>())
    {
        MainWidgetRef->HandleRoundUpdate(GS->GetCurrentRound());
    }
    else
    {
        MainWidgetRef->HandleRoundUpdate(1);
    }
}