#include "HamaPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Hama.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "LastStandLegacyGameState.h"
#include "HamaMainWidget.h"
#include "RecoilComponent.h"

AHamaPlayerController::AHamaPlayerController()
{
    RecoilComponent = CreateDefaultSubobject<URecoilComponent>(TEXT("RecoilComponent"));
}

void AHamaPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (IsLocalController())
    {
        CheckAndBindUI();
    }
}

void AHamaPlayerController::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    if (IsLocalController())
    {
        CheckAndBindUI();
    }
}

void AHamaPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (UEnhancedInputComponent* EnhancedInput = CastChecked<UEnhancedInputComponent>(InputComponent))
    {
        if (SpectateNextAction)
        {
            EnhancedInput->BindAction(SpectateNextAction, ETriggerEvent::Started, this, &AHamaPlayerController::SpectateNextActionPressed);
        }
    }
}

void AHamaPlayerController::OnPossess(APawn* InPawn)
{
    bAutoManageActiveCameraTarget = true;
    CurrentSpectateTarget = nullptr;

    ResetIgnoreMoveInput();
    ResetIgnoreLookInput();

    if (HasAuthority())
    {
        GetWorldTimerManager().ClearTimer(SpectateValidationTimerHandle);
    }

    Super::OnPossess(InPawn);
}

void AHamaPlayerController::AcknowledgePossession(APawn* P)
{
    Super::AcknowledgePossession(P);

    AHama* HamaPawn = Cast<AHama>(P);
    if (!IsLocalController() || !HamaPawn) return;

    bAutoManageActiveCameraTarget = true;
    SetInputMode(FInputModeGameOnly());
    bShowMouseCursor = false;

    SafeRemoveMappingContext(SpectatorMappingContext);

    if (HamaPawn->DefaultMappingContext)
    {
        ActiveGameplayContext = HamaPawn->DefaultMappingContext;
        SafeAddMappingContext(HamaPawn->DefaultMappingContext, 0);
    }

    CheckAndBindUI();
}

void AHamaPlayerController::ClientRestart_Implementation(APawn* NewPawn)
{
    Super::ClientRestart_Implementation(NewPawn);

    if (!NewPawn || !IsLocalController()) return;

    bAutoManageActiveCameraTarget = true;
    ChangeState(NAME_Playing);

    if (PlayerInput)
    {
        PlayerInput->FlushPressedKeys();
    }

    ResetIgnoreMoveInput();
    ResetIgnoreLookInput();

    SafeRemoveMappingContext(SpectatorMappingContext);

    if (AHama* HamaPawn = Cast<AHama>(NewPawn))
    {
        ActiveGameplayContext = HamaPawn->DefaultMappingContext;
        SafeRemoveMappingContext(HamaPawn->DefaultMappingContext);
        SafeAddMappingContext(HamaPawn->DefaultMappingContext, 0);
        SetViewTarget(HamaPawn);
    }

    SetControlRotation(NewPawn->GetActorRotation());
    SetInputMode(FInputModeGameOnly());
    bShowMouseCursor = false;

    CheckAndBindUI();
}

void AHamaPlayerController::OnOwnerDied()
{
    if (!HasAuthority()) return;

    UnPossess();

    bAutoManageActiveCameraTarget = false;
    ChangeState(NAME_Spectating);

    if (PlayerState)
    {
        PlayerState->SetIsSpectator(true);
    }

    AHama* TargetCandidate = GetNextSpectatorTarget(nullptr);
    CurrentSpectateTarget = TargetCandidate;

    if (TargetCandidate)
    {
        SetViewTarget(TargetCandidate);
    }

    ClientEnterSpectating(TargetCandidate);

    GetWorldTimerManager().SetTimer(SpectateValidationTimerHandle, this, &AHamaPlayerController::ValidateSpectateTarget, 0.25f, true);
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
        Client_SetSpectateTarget(Target);
    }
}

void AHamaPlayerController::ValidateSpectateTarget()
{
    if (!HasAuthority() || !IsInState(NAME_Spectating))
    {
        GetWorldTimerManager().ClearTimer(SpectateValidationTimerHandle);
        return;
    }

    AHama* Current = CurrentSpectateTarget;
    if (!IsValid(Current) || Current->bIsDead || Current->IsDowned())
    {
        AHama* NextTarget = GetNextSpectatorTarget(Current);
        CurrentSpectateTarget = NextTarget;

        if (NextTarget)
        {
            SetViewTarget(NextTarget);
            Client_SetSpectateTarget(NextTarget);
        }
    }
}

AHama* AHamaPlayerController::GetNextSpectatorTarget(AHama* CurrentTarget)
{
    UWorld* World = GetWorld();
    if (!World) return nullptr;

    AGameStateBase* GS = World->GetGameState();
    if (!GS) return nullptr;    

    TArray<AHama*, TInlineAllocator<4>> ValidTargets;

    for (APlayerState* PS : GS->PlayerArray)
    {
        if (!PS || PS == PlayerState) continue;

        APawn* PawnCandidate = PS->GetPawn();
        AHama* Candidate = Cast<AHama>(PawnCandidate);

        if (IsValid(Candidate) && !Candidate->bIsDead && !Candidate->IsDowned())
        {
            ValidTargets.Add(Candidate);
        }
    }

    if (ValidTargets.Num() == 0) return nullptr;

    if (!CurrentTarget) return ValidTargets[0];

    int32 CurrentIndex = ValidTargets.Find(CurrentTarget);
    if (CurrentIndex != INDEX_NONE && CurrentIndex + 1 < ValidTargets.Num())
    {
        return ValidTargets[CurrentIndex + 1];
    }

    return ValidTargets[0];
}

void AHamaPlayerController::ClientEnterSpectating_Implementation(AHama* Target)
{
    if (!IsLocalController()) return;

    ChangeState(NAME_Spectating);
    bAutoManageActiveCameraTarget = false;

    if (PlayerInput)
    {
        PlayerInput->FlushPressedKeys();
    }

    if (ActiveGameplayContext)
    {
        SafeRemoveMappingContext(ActiveGameplayContext);
    }

    SafeAddMappingContext(SpectatorMappingContext, 10);

    AHama* FinalTarget = IsValid(Target) ? Target : GetNextSpectatorTarget(nullptr);
    CurrentSpectateTarget = FinalTarget;

    if (IsValid(FinalTarget))
    {
        SetViewTarget(FinalTarget);
    }

    bShowMouseCursor = false;
    SetInputMode(FInputModeGameOnly());
}

void AHamaPlayerController::Client_SetSpectateTarget_Implementation(AHama* Target)
{
    if (!IsLocalController()) return;

    AHama* FinalTarget = IsValid(Target) ? Target : GetNextSpectatorTarget(nullptr);
    CurrentSpectateTarget = FinalTarget;

    if (IsValid(FinalTarget) && GetViewTarget() != FinalTarget)
    {
        SetViewTarget(FinalTarget);
        FinalTarget->UpdateCameraAndMeshVisibility();
    }
}

void AHamaPlayerController::SafeAddMappingContext(UInputMappingContext* Context, int32 Priority)
{
    if (!Context || !IsLocalController()) return;

    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
        {
            Subsystem->AddMappingContext(Context, Priority);
        }
    }
}

void AHamaPlayerController::SafeRemoveMappingContext(UInputMappingContext* Context)
{
    if (!Context || !IsLocalController()) return;

    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
        {
            Subsystem->RemoveMappingContext(Context);
        }
    }
}

void AHamaPlayerController::CheckAndBindUI()
{
    if (!IsLocalController()) return;

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
        if (ALastStandLegacyGameState* LSLGameState = Cast<ALastStandLegacyGameState>(GS))
        {
            MainWidgetRef->BindGameState(LSLGameState);
        }
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