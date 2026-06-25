#include "LastStandLegacyGameState.h"
#include "Hama.h"
#include "Net/UnrealNetwork.h"

ALastStandLegacyGameState::ALastStandLegacyGameState()
{
    SetNetUpdateFrequency(1.f);
}

void ALastStandLegacyGameState::BeginPlay()
{
    Super::BeginPlay();

    if (!HasAuthority()) return;

    // ── تەنها سێرڤەر ئەم تایمەرە دەسەلمێنێت ──
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

    DOREPLIFETIME(ALastStandLegacyGameState, bIsGlobalBulletStormActive);
    DOREPLIFETIME(ALastStandLegacyGameState, bIsDoublePointsActive);
    DOREPLIFETIME(ALastStandLegacyGameState, bHasInstaKill);
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
        GetWorldTimerManager().ClearTimer(BulletStormTimer);
    }
}

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
        GetWorldTimerManager().ClearTimer(DoublePointTimer);
    }
}

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
        GetWorldTimerManager().ClearTimer(InstaKillTimer);
    }
}

void ALastStandLegacyGameState::OnRep_DoublePoints()
{
}

void ALastStandLegacyGameState::OnRep_InstaKill()
{
}

void ALastStandLegacyGameState::OnRep_GlobalBulletStorm()
{
    if (bIsGlobalBulletStormActive)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, TEXT("CLIENT: BulletStorm is Active!"));
    }
    else
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("CLIENT: BulletStorm Ended!"));
    }
}