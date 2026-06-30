#include "LastStandLegacyGameState.h"
#include "Hama.h"
#include "HamaMainWidget.h" // +++ گرنگە بۆ ناسینەوەی UI +++
#include "Net/UnrealNetwork.h"

ALastStandLegacyGameState::ALastStandLegacyGameState()
{
    SetNetUpdateFrequency(1.f);
}

void ALastStandLegacyGameState::BeginPlay()
{
    Super::BeginPlay();

    if (!HasAuthority()) return;

    // 0.25f = 4 جار لە چرکەیەکدا، بە یەک سکان هەموو زۆمبیەکانی جیهان خزمەت دەکات
    GetWorldTimerManager().SetTimer(
        TargetCacheTimerHandle,
        this,
        &ALastStandLegacyGameState::RefreshValidTargets,
        0.25f,
        true);
}

void ALastStandLegacyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ALastStandLegacyGameState, CurrentRound);
    DOREPLIFETIME(ALastStandLegacyGameState, bIsGlobalBulletStormActive);
    DOREPLIFETIME(ALastStandLegacyGameState, bIsDoublePointsActive);
    DOREPLIFETIME(ALastStandLegacyGameState, bHasInstaKill);
    DOREPLIFETIME(ALastStandLegacyGameState, bIsAdrenalineActive);
    DOREPLIFETIME(ALastStandLegacyGameState, bIsPowerOn);
    DOREPLIFETIME_CONDITION(ALastStandLegacyGameState, bIsSoloMatch, COND_InitialOnly);
}

// ── فەنکشنە نوێیەکانی سیستەمی ڕاوەند ──
void ALastStandLegacyGameState::SetCurrentRound(int32 NewRound)
{
    if (!HasAuthority()) return;
    CurrentRound = NewRound;
    ForceNetUpdate();
    // نوێکردنەوەی شاشەی کەسی هۆست (سێرڤەر) ڕاستەوخۆ
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        if (AHama* HamaChar = Cast<AHama>(PC->GetPawn()))
        {
            if (HamaChar->MainWidgetRef)
            {
                HamaChar->MainWidgetRef->UpdateRoundText(CurrentRound);
            }
        }
    }
}

void ALastStandLegacyGameState::OnRep_CurrentRound()
{
    // کاتێک سێرڤەر ڕاوند دەگۆڕێت، ئەمە لەسەر کۆمپیوتەری کڵایەنتەکان لێدەدات و شاشەیان ئەپدێت دەکات
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        if (AHama* HamaChar = Cast<AHama>(PC->GetPawn()))
        {
            if (HamaChar->MainWidgetRef)
            {
                HamaChar->MainWidgetRef->UpdateRoundText(CurrentRound);
            }
        }
    }
}

void ALastStandLegacyGameState::RefreshValidTargets()
{
    ValidTargets.Reset();

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (!It->IsValid()) continue;

        APawn* Pawn = It->Get()->GetPawn();
        if (!Pawn) continue;

        if (AHama* H = Cast<AHama>(Pawn))
        {
            if (H->IsGhost() || H->IsDowned()) continue;
        }

        ValidTargets.Add(Pawn);
    }
}

// ------------------- ADRENALINE (BLITZ) -------------------
void ALastStandLegacyGameState::StartTeamAdrenaline(float Duration)
{
    if (HasAuthority())
    {
        bIsAdrenalineActive = true;
        ForceNetUpdate();
        OnRep_Adrenaline();
        GetWorldTimerManager().SetTimer(AdrenalineTimerHandle, this, &ALastStandLegacyGameState::EndTeamAdrenaline, Duration, false);
    }
}

void ALastStandLegacyGameState::EndTeamAdrenaline()
{
    if (HasAuthority())
    {
        bIsAdrenalineActive = false;
        ForceNetUpdate();
        OnRep_Adrenaline();
    }
}

// ------------------- BULLET STORM -------------------
void ALastStandLegacyGameState::StartGlobalBulletStorm(float Duration)
{
    if (HasAuthority())
    {
        bIsGlobalBulletStormActive = true;
        ForceNetUpdate();
        OnRep_GlobalBulletStorm();
        GetWorldTimerManager().SetTimer(BulletStormTimer, this, &ALastStandLegacyGameState::EndGlobalBulletStorm, Duration, false);
    }
}

void ALastStandLegacyGameState::EndGlobalBulletStorm()
{
    if (HasAuthority())
    {
        bIsGlobalBulletStormActive = false;
        ForceNetUpdate();
        OnRep_GlobalBulletStorm();
    }
}

// ------------------- DOUBLE POINTS -------------------
void ALastStandLegacyGameState::StartDoublePoints(float Duration)
{
    if (HasAuthority())
    {
        bIsDoublePointsActive = true;
        ForceNetUpdate();
        OnRep_DoublePoints();
        GetWorldTimerManager().SetTimer(DoublePointTimer, this, &ALastStandLegacyGameState::EndDoublePoints, Duration, false);
    }
}

void ALastStandLegacyGameState::EndDoublePoints()
{
    if (HasAuthority())
    {
        bIsDoublePointsActive = false;
        ForceNetUpdate();
        OnRep_DoublePoints();
    }
}

// ------------------- INSTA KILL -------------------
void ALastStandLegacyGameState::StartinstaKill(float Duration)
{
    if (HasAuthority())
    {
        bHasInstaKill = true;
        ForceNetUpdate();
        OnRep_InstaKill();
        GetWorldTimerManager().SetTimer(InstaKillTimer, this, &ALastStandLegacyGameState::EndInstaKill, Duration, false);
    }
}

void ALastStandLegacyGameState::EndInstaKill()
{
    if (HasAuthority())
    {
        bHasInstaKill = false;
        ForceNetUpdate();
        OnRep_InstaKill();
    }
}

// ------------------- ON REP FUNCTIONS -------------------
void ALastStandLegacyGameState::OnRep_Adrenaline()
{
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, bIsAdrenalineActive ? FColor::Orange : FColor::Red, bIsAdrenalineActive ? TEXT("CLIENT: Blitz Adrenaline Active!") : TEXT("CLIENT: Blitz Ended!"));
}

void ALastStandLegacyGameState::OnRep_DoublePoints()
{
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, bIsDoublePointsActive ? FColor::Yellow : FColor::Red, bIsDoublePointsActive ? TEXT("CLIENT: Double Points Active!") : TEXT("CLIENT: Double Points Ended!"));
}

void ALastStandLegacyGameState::OnRep_InstaKill()
{
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, bHasInstaKill ? FColor::Magenta : FColor::Red, bHasInstaKill ? TEXT("CLIENT: Insta-Kill Active!") : TEXT("CLIENT: Insta-Kill Ended!"));
}

void ALastStandLegacyGameState::OnRep_GlobalBulletStorm()
{
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, bIsGlobalBulletStormActive ? FColor::Green : FColor::Red, bIsGlobalBulletStormActive ? TEXT("CLIENT: BulletStorm is Active!") : TEXT("CLIENT: BulletStorm Ended!"));
}