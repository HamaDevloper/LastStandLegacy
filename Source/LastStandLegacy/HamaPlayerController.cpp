#include "HamaPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Hama.h"
#include "GameFramework/GameStateBase.h"
#include "HamaMainWidget.h"

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

void AHamaPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
    {
        if (SpectateNextAction)
        {
            EnhancedInput->BindAction(SpectateNextAction, ETriggerEvent::Started, this, &AHamaPlayerController::SpectateNextActionPressed);
        }
    }
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
            Subsystem->ClearAllMappings();

            if (AHama* HamaPawn = Cast<AHama>(P))
            {
                if (HamaPawn->DefaultMappingContext)
                {
                    Subsystem->AddMappingContext(HamaPawn->DefaultMappingContext, 0);
                }
            }
        }
    }
}

void AHamaPlayerController::BeginSpectatingState()
{
    Super::BeginSpectatingState();

    if (IsLocalController())
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
        {
            Subsystem->ClearAllMappings();

            if (SpectatorMappingContext)
            {
                Subsystem->AddMappingContext(SpectatorMappingContext, 0);
            }
        }
    }
}

void AHamaPlayerController::SpectateNextActionPressed(const FInputActionValue& Value)
{
    if (IsInState(NAME_Spectating))
    {
        Server_SpectateNext();
    }
}

void AHamaPlayerController::Server_SpectateNext_Implementation()
{
    if (!HasAuthority() || !IsInState(NAME_Spectating)) return;

    AHama* Target = GetNextSpectatorTarget(CurrentSpectateTarget);
    if (Target)
    {
        CurrentSpectateTarget = Target;
        SetViewTarget(Target);
    }
}

void AHamaPlayerController::OnOwnerDied()
{
    if (!HasAuthority()) return;

    UnPossess();
    ChangeState(NAME_Spectating);
    ClientGotoState(NAME_Spectating);

    UWorld* World = GetWorld();
    if (!World) return;

    ALastStandLegacyGameState* GS = Cast<ALastStandLegacyGameState>(World->GetGameState());
    if (!GS) return;

    for (APlayerState* PS : GS->PlayerArray)
    {
        if (!PS) continue;

        AHama* Candidate = Cast<AHama>(PS->GetPawn());
        if (IsValid(Candidate) && !Candidate->bIsDead && !Candidate->IsDowned())
        {
            CurrentSpectateTarget = Candidate;
            SetViewTarget(Candidate);
            break;
        }
    }
}

AHama* AHamaPlayerController::GetNextSpectatorTarget(AHama* CurrentTarget)
{
    UWorld* World = GetWorld();
    if (!World) return nullptr;

    AGameStateBase* GS = World->GetGameState();
    if (!GS) return nullptr;

    bool bFoundCurrent = (CurrentTarget == nullptr);

    for (APlayerState* PS : GS->PlayerArray)
    {
        if (!PS) continue;

        AHama* Candidate = Cast<AHama>(PS->GetPawn());
        if (IsValid(Candidate) && !Candidate->bIsDead && !Candidate->IsDowned())
        {
            if (bFoundCurrent)
            {
                return Candidate;
            }

            if (Candidate == CurrentTarget)
            {
                bFoundCurrent = true;
            }
        }
    }

    if (CurrentTarget != nullptr)
    {
        return GetNextSpectatorTarget(nullptr);
    }

    return nullptr;
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

    if (AGameStateBase* GS = GetWorld()->GetGameState())
    {
        MainWidgetRef->BindGameState(Cast<ALastStandLegacyGameState>(GS));
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