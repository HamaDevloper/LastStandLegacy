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
#include "EngineUtils.h"
#include "Algo/Sort.h"

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

void AHamaPlayerController::OnRep_Pawn()
{
    if (GetPawn() == nullptr)
    {
        bAutoManageActiveCameraTarget = false;
    }

    Super::OnRep_Pawn();
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
    bShowMouseCursor = false;

    SetInputMode(FInputModeGameOnly());

    SafeRemoveMappingContext(SpectatorMappingContext);

    if (HamaPawn->DefaultMappingContext)
    {
        SafeRemoveMappingContext(ActiveGameplayContext);

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
    CurrentSpectateTarget = nullptr;
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

    bAutoManageActiveCameraTarget = false;

    UnPossess();
    ChangeState(NAME_Spectating);

    if (PlayerState)
    {
        PlayerState->SetIsSpectator(true);
    }

    AHama* TargetCandidate = GetCycledSpectatorTarget(nullptr, 1);
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
    RequestSpectateCycle(1);
}

void AHamaPlayerController::SpectatePreviousActionPressed(const FInputActionValue& Value)
{
    RequestSpectateCycle(-1);
}

void AHamaPlayerController::RequestSpectateCycle(int32 Direction)
{
    if (!IsInState(NAME_Spectating)) return;

    UWorld* World = GetWorld();
    if (!World) return;

    const float CurrentTime = World->GetTimeSeconds();
    if (CurrentTime - LastClientSwitchTime < SpectateSwitchCooldown)
    {
        return;
    }
    LastClientSwitchTime = CurrentTime;

    if (IsLocalController() && !HasAuthority())
    {
        if (AHama* PredictedTarget = GetCycledSpectatorTarget(CurrentSpectateTarget, Direction))
        {
            ApplyLocalSpectateViewTarget(PredictedTarget);
        }
    }

    Server_CycleSpectateTarget(Direction);
}

void AHamaPlayerController::Server_CycleSpectateTarget_Implementation(int32 Direction)
{
    if (!HasAuthority() || !IsInState(NAME_Spectating)) return;

    UWorld* World = GetWorld();
    if (!World) return;

    const float CurrentTime = World->GetTimeSeconds();
    if (CurrentTime - LastServerSwitchTime < (SpectateSwitchCooldown * 0.8f))
    {
        return;
    }
    LastServerSwitchTime = CurrentTime;

    const int32 Step = (Direction >= 0) ? 1 : -1;
    AHama* Target = GetCycledSpectatorTarget(CurrentSpectateTarget, Step);

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
        AHama* NextTarget = GetCycledSpectatorTarget(Current, 1);
        CurrentSpectateTarget = NextTarget;

        if (NextTarget)
        {
            SetViewTarget(NextTarget);
            Client_SetSpectateTarget(NextTarget);
        }
    }
}

AHama* AHamaPlayerController::GetCycledSpectatorTarget(AHama* CurrentTarget, int32 Direction) const
{
    UWorld* World = GetWorld();
    if (!World) return nullptr;

    TArray<AHama*, TInlineAllocator<4>> ValidTargets;

    for (AHama* Candidate : TActorRange<AHama>(World))
    {
        if (!IsValid(Candidate) || Candidate == GetPawn()) continue;
        if (Candidate->bIsDead || Candidate->IsDowned()) continue;

        if (PlayerState && Candidate->GetPlayerState() == PlayerState) continue;

        ValidTargets.Add(Candidate);
    }

    const int32 NumTargets = ValidTargets.Num();
    if (NumTargets == 0) return nullptr;

    Algo::Sort(ValidTargets, [](const AHama* A, const AHama* B)
        {
            const APlayerState* PSA = A->GetPlayerState();
            const APlayerState* PSB = B->GetPlayerState();
            if (PSA && PSB)
            {
                return PSA->GetPlayerId() < PSB->GetPlayerId();
            }
            return A->GetName() < B->GetName();
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
    if (!LocalPlayer)
    {
        return;
    }

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

    if (UWorld* World = GetWorld())
    {
        if (AGameStateBase* GS = World->GetGameState())
        {
            if (ALastStandLegacyGameState* LSLGameState = Cast<ALastStandLegacyGameState>(GS))
            {
                MainWidgetRef->BindGameState(LSLGameState);
            }
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