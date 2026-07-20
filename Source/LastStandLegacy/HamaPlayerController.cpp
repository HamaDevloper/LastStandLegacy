#include "HamaPlayerController.h"
#include "HamaMainWidget.h"
#include "HamaPlayerState.h"
#include "Hama.h"
#include "Blueprint/UserWidget.h"
#include "LastStandLegacyGameState.h"

void AHamaPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (IsLocalController()) CheckAndBindUI();
}

void AHamaPlayerController::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    if (IsLocalController()) CheckAndBindUI();
}

void AHamaPlayerController::AcknowledgePossession(APawn* P)
{
    Super::AcknowledgePossession(P);
    if (IsLocalController()) CheckAndBindUI();
}

void AHamaPlayerController::CheckAndBindUI()
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

    if (ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>())
    {
        MainWidgetRef->BindGameState(GS);
    }

    if (AHamaPlayerState* PS = GetPlayerState<AHamaPlayerState>())
    {
        MainWidgetRef->BindPlayerState(PS);
    }

    if (AHama* HamaChar = Cast<AHama>(GetPawn()))
    {
        MainWidgetRef->BindCharacter(HamaChar);
    }
}