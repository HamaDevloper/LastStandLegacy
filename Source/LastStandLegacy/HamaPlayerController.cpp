#include "HamaPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Hama.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "LastStandLegacyGameState.h"
#include "HamaPlayerState.h"
#include "HamaMainWidget.h"
#include "RecoilComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Algo/Sort.h"

AHamaPlayerController::AHamaPlayerController()
{
    RecoilComponent = CreateDefaultSubobject<URecoilComponent>(TEXT("RecoilComponent"));
    LastClientSwitchTime = -100.0f;
    LastServerSwitchTime = -100.0f;
}

void AHamaPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (IsLocalController())
    {
        if (UWorld* World = GetWorld())
        {
            World->GameStateSetEvent.AddUObject(this, &AHamaPlayerController::HandleGameStateSet);
        }
        InitializeLocalPawn(GetPawn());
        CheckAndBindUI();
    }
}

void AHamaPlayerController::ReceivedPlayer()
{
    Super::ReceivedPlayer();

    if (IsLocalController())
    {
        InitializeLocalPawn(GetPawn());
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

void AHamaPlayerController::OnRep_Pawn()
{
    APawn* CurrentPawn = GetPawn();
    if (!CurrentPawn)
    {
        bAutoManageActiveCameraTarget = false;
    }

    Super::OnRep_Pawn();

    if (IsLocalController() && CurrentPawn)
    {
        InitializeLocalPawn(CurrentPawn);
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

        if (SpectatePreviousAction)
        {
            EnhancedInput->BindAction(SpectatePreviousAction, ETriggerEvent::Started, this, &AHamaPlayerController::SpectatePreviousActionPressed);
        }
    }
}

void AHamaPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    bAutoManageActiveCameraTarget = true;
    CurrentSpectateTarget = nullptr;

    EnableInput(this);
    bBlockInput = false;
    ResetIgnoreMoveInput();
    ResetIgnoreLookInput();

    if (HasAuthority())
    {
        GetWorldTimerManager().ClearTimer(SpectateValidationTimerHandle);

        if (PlayerState)
        {
            PlayerState->SetIsSpectator(false);
        }

        if (IsLocalController())
        {
            InitializeLocalPawn(InPawn);
        }
    }
}

void AHamaPlayerController::AcknowledgePossession(APawn* P)
{
    Super::AcknowledgePossession(P);

    if (IsLocalController() && P)
    {
        InitializeLocalPawn(P);
    }
}

void AHamaPlayerController::ClientRestart_Implementation(APawn* NewPawn)
{
    Super::ClientRestart_Implementation(NewPawn);

    if (!IsLocalController() || !NewPawn) return;

    if (PlayerInput)
    {
        PlayerInput->FlushPressedKeys();
    }

    SetControlRotation(NewPawn->GetActorRotation());
    InitializeLocalPawn(NewPawn);
}

void AHamaPlayerController::InitializeLocalPawn(APawn* InPawn)
{
    if (!IsLocalController()) return;

    AHama* HamaPawn = Cast<AHama>(InPawn);
    if (!IsValid(HamaPawn)) return;

    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    if (!LocalPlayer) return;

    bAutoManageActiveCameraTarget = true;
    CurrentSpectateTarget = nullptr;

    if (!IsInState(NAME_Playing))
    {
        ChangeState(NAME_Playing);
    }

    if (GetLocalRole() < ROLE_Authority && HamaPawn->GetController() != this)
    {
        HamaPawn->Controller = this;
    }

    if (!HamaPawn->InputComponent)
    {
        HamaPawn->PawnClientRestart();
    }

    if (AcknowledgedPawn != HamaPawn)
    {
        AcknowledgePossession(HamaPawn);
    }

    EnableInput(this);
    bBlockInput = false;
    ResetIgnoreMoveInput();
    ResetIgnoreLookInput();

    SafeRemoveMappingContext(SpectatorMappingContext);

    if (HamaPawn->DefaultMappingContext)
    {
        if (ActiveGameplayContext && ActiveGameplayContext != HamaPawn->DefaultMappingContext)
        {
            SafeRemoveMappingContext(ActiveGameplayContext);
        }

        ActiveGameplayContext = HamaPawn->DefaultMappingContext;
        SafeAddMappingContext(ActiveGameplayContext, 0);
    }

    if (GetViewTarget() != HamaPawn)
    {
        SetViewTarget(HamaPawn);
    }

    bShowMouseCursor = false;
    SetInputMode(FInputModeGameOnly());

    CheckAndBindUI();
}

void AHamaPlayerController::OnOwnerDied()
{
    if (!HasAuthority()) return;

    bAutoManageActiveCameraTarget = false;

    UnPossess();
    ChangeState(NAME_Spectating);

    if (PlayerState)
    {
        PlayerState->SetIsSpectator(true);
    }

    AHama* TargetCandidate = GetCycledSpectatorTarget(nullptr, 1);
    CurrentSpectateTarget = TargetCandidate;


    if (IsLocalController() && TargetCandidate)
    {
        ApplyLocalSpectateViewTarget(TargetCandidate);
    }

    ClientEnterSpectating(TargetCandidate);

    GetWorldTimerManager().SetTimer(
        SpectateValidationTimerHandle,
        this,
        &AHamaPlayerController::ValidateSpectateTarget,
        0.5f,
        true
    );
}

void AHamaPlayerController::SpectateNextActionPressed(const FInputActionValue& Value)
{
    RequestSpectateCycle(1);
}

void AHamaPlayerController::SpectatePreviousActionPressed(const FInputActionValue& Value)
{
    RequestSpectateCycle(-1);
}

void AHamaPlayerController::RequestSpectateCycle(int32 Direction)
{
    if (!IsInState(NAME_Spectating) || Direction == 0) return;

    UWorld* World = GetWorld();
    if (!World) return;

    const float CurrentTime = World->GetTimeSeconds();
    if (CurrentTime - LastClientSwitchTime < SpectateSwitchCooldown)
    {
        return;
    }

    AHama* PredictedTarget = GetCycledSpectatorTarget(CurrentSpectateTarget, Direction);
    if (!PredictedTarget || PredictedTarget == CurrentSpectateTarget)
    {
        return;
    }

    LastClientSwitchTime = CurrentTime;

    if (IsLocalController() && !HasAuthority())
    {
        ApplyLocalSpectateViewTarget(PredictedTarget);
    }

    Server_CycleSpectateTarget(Direction > 0 ? 1 : -1);
}

void AHamaPlayerController::Server_CycleSpectateTarget_Implementation(int32 Direction)
{
    if (!HasAuthority() || !IsInState(NAME_Spectating) || Direction == 0) return;

    UWorld* World = GetWorld();
    if (!World) return;

    const float CurrentTime = World->GetTimeSeconds();
    if (CurrentTime - LastServerSwitchTime < (SpectateSwitchCooldown * 0.8f))
    {
        return;
    }

    const int32 Step = (Direction > 0) ? 1 : -1;
    AHama* Target = GetCycledSpectatorTarget(CurrentSpectateTarget, Step);

    if (Target && Target != CurrentSpectateTarget)
    {
        LastServerSwitchTime = CurrentTime;
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
        AHama* NextTarget = GetCycledSpectatorTarget(Current, 1);
        if (NextTarget != CurrentSpectateTarget)
        {
            CurrentSpectateTarget = NextTarget;

            if (NextTarget)
            {
                SetViewTarget(NextTarget);
                Client_SetSpectateTarget(NextTarget);
            }
        }
    }
}

AHama* AHamaPlayerController::GetCycledSpectatorTarget(AHama* CurrentTarget, int32 Direction) const
{
    UWorld* World = GetWorld();
    if (!World) return nullptr;

    AGameStateBase* GS = World->GetGameState();
    if (!GS) return nullptr;

    TArray<AHama*, TInlineAllocator<4>> ValidTargets;

    for (APlayerState* PS : GS->PlayerArray)
    {
        if (!PS || PS == PlayerState) continue;

        AHama* Candidate = Cast<AHama>(PS->GetPawn());
        if (!IsValid(Candidate) || Candidate == GetPawn()) continue;
        if (Candidate->bIsDead || Candidate->IsDowned()) continue;

        ValidTargets.Add(Candidate);
    }

    const int32 NumTargets = ValidTargets.Num();
    if (NumTargets == 0) return nullptr;

    Algo::Sort(ValidTargets, [](const AHama* A, const AHama* B)
        {
            return A->GetPlayerState()->GetPlayerId() < B->GetPlayerState()->GetPlayerId();
        });

    if (!IsValid(CurrentTarget))
    {
        return ValidTargets[0];
    }

    const int32 CurrentIndex = ValidTargets.Find(CurrentTarget);
    if (CurrentIndex == INDEX_NONE)
    {
        return ValidTargets[0];
    }

    const int32 Step = (Direction >= 0) ? 1 : -1;
    const int32 NextIndex = (CurrentIndex + Step + NumTargets) % NumTargets;

    return ValidTargets[NextIndex];
}

void AHamaPlayerController::ApplyLocalSpectateViewTarget(AHama* NewTarget)
{
    if (!IsValid(NewTarget)) return;

    AHama* PreviousTarget = CurrentSpectateTarget;
    CurrentSpectateTarget = NewTarget;

    if (GetViewTarget() != NewTarget)
    {
        SetViewTarget(NewTarget);
    }

    if (IsValid(PreviousTarget) && PreviousTarget != NewTarget)
    {
        PreviousTarget->UpdateCameraAndMeshVisibility();
    }

    NewTarget->UpdateCameraAndMeshVisibility();
}

void AHamaPlayerController::ClientEnterSpectating_Implementation(AHama* Target)
{
    if (!IsLocalController()) return;

    bAutoManageActiveCameraTarget = false;
    ChangeState(NAME_Spectating);

    if (PlayerInput)
    {
        PlayerInput->FlushPressedKeys();
    }

    if (SpectatorMappingContext)
    {
        if (ActiveGameplayContext)
        {
            SafeRemoveMappingContext(ActiveGameplayContext);
        }
        SafeAddMappingContext(SpectatorMappingContext, 10);
    }

    AHama* FinalTarget = IsValid(Target) ? Target : GetCycledSpectatorTarget(nullptr, 1);
    ApplyLocalSpectateViewTarget(FinalTarget);

    bShowMouseCursor = false;
    SetInputMode(FInputModeGameOnly());
}

void AHamaPlayerController::Client_SetSpectateTarget_Implementation(AHama* Target)
{
    if (!IsLocalController()) return;

    AHama* FinalTarget = IsValid(Target) ? Target : GetCycledSpectatorTarget(nullptr, 1);
    ApplyLocalSpectateViewTarget(FinalTarget);
}

void AHamaPlayerController::SafeAddMappingContext(UInputMappingContext* Context, int32 Priority)
{
    if (!Context || !IsLocalController()) return;

    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    if (!LocalPlayer) return;

    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
    {
        if (!Subsystem->HasMappingContext(Context))
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

void AHamaPlayerController::HandleGameStateSet(AGameStateBase* NewGameState)
{
    CheckAndBindUI();
}

void AHamaPlayerController::CheckAndBindUI()
{
    if (!IsLocalController() || !GetLocalPlayer()) return;

    if (!MainWidgetRef && MainWidgetClass)
    {
        MainWidgetRef = CreateWidget<UHamaMainWidget>(this, MainWidgetClass);
        if (MainWidgetRef)
        {
            MainWidgetRef->AddToViewport();
        }
    }

    if (!MainWidgetRef) return;

    if (UWorld* World = GetWorld())
    {
        if (ALastStandLegacyGameState* LSLGameState = World->GetGameState<ALastStandLegacyGameState>())
        {
            if (BoundGameState.Get() != LSLGameState)
            {
                BoundGameState = LSLGameState;
                MainWidgetRef->BindGameState(LSLGameState);
            }
        }
    }

    if (AHamaPlayerState* PS = GetPlayerState<AHamaPlayerState>())
    {
        if (BoundPlayerState.Get() != PS)
        {
            BoundPlayerState = PS;
            MainWidgetRef->BindPlayerState(PS);
        }
    }

    if (AHama* HamaChar = Cast<AHama>(GetPawn()))
    {
        if (BoundCharacter.Get() != HamaChar)
        {
            BoundCharacter = HamaChar;
            MainWidgetRef->BindCharacter(HamaChar);
        }
    }
}