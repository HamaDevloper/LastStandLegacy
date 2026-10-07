#include "HamaPlayerController.h"
#include "HamaMainWidget.h"
#include "HamaPlayerState.h"
#include "Hama.h"
#include "Blueprint/UserWidget.h"
#include "LastStandLegacyGameState.h"
#include "EnhancedInputSubsystems.h"

AHamaPlayerController::AHamaPlayerController()
{
    RecoilComponent = CreateDefaultSubobject<URecoilComponent>(TEXT("RecoilComponent"));
}

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

    if (IsLocalController())
    {
        CheckAndBindUI();

        FInputModeGameOnly GameInputMode;
        SetInputMode(GameInputMode);
        bShowMouseCursor = false;

        ResetIgnoreMoveInput();
        ResetIgnoreLookInput();

        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
        {
            if (AHama* HamaPawn = Cast<AHama>(P))
            {
                if (HamaPawn->DefaultMappingContext)
                {
                    Subsystem->RemoveMappingContext(HamaPawn->DefaultMappingContext);
                    Subsystem->AddMappingContext(HamaPawn->DefaultMappingContext, 0);
                }
            }
        }
    }
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

void AHamaPlayerController::OnOwnerDied()
{
    if (!HasAuthority()) return;

    UnPossess();
    ChangeState(NAME_Spectating);
    ClientGotoState(NAME_Spectating);

    AHama* Target = GetNextSpectatorTarget(nullptr);
    if (Target)
    {
        CurrentSpectateTarget = Target;

        SetViewTarget(Target);

        FViewTargetTransitionParams Params;
        Params.BlendTime = 0.0f;
        ClientSetViewTarget(Target, Params);
    }
}

void AHamaPlayerController::ClientGotoState_Implementation(FName NewState, uint8 GroupID)
{
    Super::ClientGotoState_Implementation(NewState, GroupID);

    if (IsLocalController() && NewState == NAME_Spectating)
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
        {
            if (DefaultInputContext)
            {
                Subsystem->RemoveMappingContext(DefaultInputContext);
            }
            if (SpectatorInputContext)
            {
                Subsystem->AddMappingContext(SpectatorInputContext, 0);
            }
        }
    }
}

AHama* AHamaPlayerController::GetNextSpectatorTarget(AHama* CurrentTarget)
{
    UWorld* World = GetWorld();
    if (!World) return nullptr;

    AGameStateBase* GS = World->GetGameState();
    if (!GS) return nullptr;

    for (APlayerState* PS : GS->PlayerArray)
    {
        if (!PS) continue;

        AHama* Candidate = Cast<AHama>(PS->GetPawn());
        if (IsValid(Candidate) && Candidate->GetController() != this && !Candidate->bIsDead && !Candidate->IsDowned())
        {
            return Candidate;
        }
    }

    return nullptr;
}