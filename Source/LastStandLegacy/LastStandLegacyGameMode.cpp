#include "LastStandLegacyGameMode.h"
#include "Zombie.h"
#include "Hama.h"
#include "BasePowerUp.h" 
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "HamaPlayerState.h"
#include "ZombieSpawnPoint.h"
#include "LastStandLegacyGameState.h"
#include "HamaAbilityComponent.h"
#include "PerkSpawnPoint.h"
#include "BasePerk.h"
#include "NavigationSystem.h"
#include "Components/CapsuleComponent.h"

ALastStandLegacyGameMode::ALastStandLegacyGameMode()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ALastStandLegacyGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);

    ActiveAbilities.Empty();
    ActiveAbilities.Add(EHamaAbilityType::BulletStorm);
    ActiveAbilities.Add(EHamaAbilityType::MedicalSupport);
    ActiveAbilities.Add(EHamaAbilityType::GhostMode);
    ActiveAbilities.Add(EHamaAbilityType::Blitz);

    for (int32 i = 0; i < ActiveAbilities.Num(); ++i)
    {
        int32 RandomIndex = FMath::RandRange(i, ActiveAbilities.Num() - 1);
        if (i != RandomIndex)
        {
            ActiveAbilities.Swap(i, RandomIndex);
        }
    }
}

void ALastStandLegacyGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    if (NewPlayer)
    {
        if (AHamaPlayerState* PS = NewPlayer->GetPlayerState<AHamaPlayerState>())
        {
            int32 CalculatedStartingPoints = BaseStartingPoints + ((CurrentRound - 1) * PointsPerRoundScaling);
            PS->SetPoints(CalculatedStartingPoints);

            if (PS->GetAssignedRole() == EHamaAbilityType::None && !ActiveAbilities.IsEmpty())
            {
                EHamaAbilityType AssignedAbility = ActiveAbilities.Pop();
                PS->SetAssignedRole(AssignedAbility);

            }
            else if (ActiveAbilities.IsEmpty())
            {
                UE_LOG(LogTemp, Warning, TEXT("No abilities left for %s!"), *NewPlayer->GetName());
            }
        }
        ALastStandLegacyGameState* GS = GetGameState<ALastStandLegacyGameState>();
        if (GS)
        {
            if (GetNumPlayers() <= 1)
            {
                GS->bIsSoloMatch = true;
            }
            else
            {
                GS->bIsSoloMatch = false;
            }
        }
    }
}

void ALastStandLegacyGameMode::BeginPlay()
{
    Super::BeginPlay();

    SpawnPoints.Empty();

    for (TActorIterator<AZombieSpawnPoint> It(GetWorld()); It; ++It)
    {
        AZombieSpawnPoint* SpawnPoint = *It;
        SpawnPoints.Add(SpawnPoint);
    }

    if (SpawnPoints.IsEmpty())
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("No SpawnPoint found! Add 'ZombieSpawn'"));
    }

    SpawnRandomPerks();

    DeadZombiesCount = 0;
    ActiveZombiesCount = 0;
    ZombiesSpawnedThisRound = 0;

    ZombiesToKill = BaseZombiesCount + ((CurrentRound - 1) * ZombiesPerRoundIncrement);

    if (ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>())
    {
        GS->SetCurrentRound(CurrentRound);
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
            FString::Printf(TEXT("Round %d Started! Zombies this round: %d"), CurrentRound, ZombiesToKill));
    }

    const float InitialInterval = GetCalculateSpawnInterval();

    GetWorldTimerManager().SetTimer(SpawnTimerHandle, this, &ALastStandLegacyGameMode::ProcessSpawning, InitialInterval, true);
}

void ALastStandLegacyGameMode::MyShufflePerks(TArray<TSubclassOf<ABasePerk>>& ArrayToShuffle)
{
    if (ArrayToShuffle.Num() <= 1) return;

    for (int32 i = ArrayToShuffle.Num() - 1; i > 0; i--)
    {
        int32 RandomIndex = FMath::RandRange(0, i);

        if (i != RandomIndex)
        {
            ArrayToShuffle.Swap(i, RandomIndex);
        }
    }
}

void ALastStandLegacyGameMode::SpawnRandomPerks()
{
    if (PerkClasses.IsEmpty()) return;

    TArray<APerkSpawnPoint*> FoundPerkPoints;
    for (TActorIterator<APerkSpawnPoint> It(GetWorld()); It; ++It)
    {
        FoundPerkPoints.Add(*It);
    }

    if (FoundPerkPoints.IsEmpty())
    {
       GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("No PerkSpawnPoints found in the map!"));
        return;
    }

    MyShufflePerks(PerkClasses);

    int32 SpawnsCount = FMath::Min(PerkClasses.Num(), FoundPerkPoints.Num());

    for (int32 i = 0; i < SpawnsCount; i++)
    {
        if (FoundPerkPoints[i] && PerkClasses[i])
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            GetWorld()->SpawnActor<ABasePerk>(
                PerkClasses[i],
                FoundPerkPoints[i]->GetActorLocation(),
                FoundPerkPoints[i]->GetActorRotation(),
                SpawnParams
            );
        }
    }
}

void ALastStandLegacyGameMode::HandleZombieDeath(AZombie* DeadZombie, AController* KillerController)
{
    if (KillerController)
    {
        APawn* KillerPawn = KillerController->GetPawn();
        if (KillerPawn)
        {
            UHamaAbilityComponent* AbilityComp = KillerPawn->FindComponentByClass<UHamaAbilityComponent>();
            if (AbilityComp)
            {
                float BasePowerReward = 15.0f / (1.0f + (CurrentRound * 0.15f));
                float PowerReward = FMath::Max(BasePowerReward, 2.0f);

                AbilityComp->AddPower(PowerReward);
            }
        }
    }

    DeadZombiesCount++;
    ActiveZombiesCount--;

    int32 ZombiesRemaining = ZombiesToKill - DeadZombiesCount;

    if (GEngine && ZombiesRemaining > 0)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Orange,
            FString::Printf(TEXT("Zombies Remaining: %d"), ZombiesRemaining));
    }

    if (KillerController != nullptr && CurrentPowerSpawn < MaxPowerSpawn)
    {
        float CurrentTime = GetWorld()->GetTimeSeconds();
        float CalculateTime = CurrentTime - CurrentPowerSpawnTime;

        if (CalculateTime >= PowerUpCooldownTime)
        {
            float RandomChance = FMath::RandRange(0.0f, 100.0f);

            if (RandomChance <= PowerUpDropChance)
            {
                SpawnPowers(DeadZombie->GetActorLocation());
                CurrentPowerSpawnTime = CurrentTime;
            }
        }
    }

    if (DeadZombiesCount >= ZombiesToKill)
    {
        CurrentRound++;

        if (ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>())
        {
            GS->SetCurrentRound(CurrentRound);
        }

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow,
                FString::Printf(TEXT("Round Completed! Next round starting in %d seconds..."), StartNexRoundDelay));
        }

        GetWorldTimerManager().SetTimer(
            RoundTransitionTimerHandle,
            this,
            &ALastStandLegacyGameMode::StartNextRound,
            StartNexRoundDelay,
            false
        );
    }
}

float ALastStandLegacyGameMode::GetCalculateSpawnInterval() const
{
    const float CalculatedInterval = BaseSpawnInterval - ((CurrentRound - 1) * SpawnIntervalDecreasePerRound);
    return FMath::Max(MinSpawnInterval, CalculatedInterval);
}

void ALastStandLegacyGameMode::StartNextRound()
{
    RespawnDeadPlayers();
    DeadZombiesCount = 0;
    ZombiesSpawnedThisRound = 0;
    ActiveZombiesCount = 0;
    CurrentPowerSpawn = 0;

    // --- لێرەدا دەبێت بە هەمان شێوەی BeginPlay حیسابی ZombiesToKill بکرێت ---
    ZombiesToKill = BaseZombiesCount + ((CurrentRound - 1) * ZombiesPerRoundIncrement);
    // --------------------------------------------------------------------------

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
            FString::Printf(TEXT("Round %d Started Zombie To Kill = %d"), CurrentRound, ZombiesToKill));
    }

    for (TActorIterator<AZombie> It(GetWorld()); It; ++It)
    {
        if (*It)
        {
            (*It)->SetStatsForRound(CurrentRound);
        }
    }

    const float NewInterval = GetCalculateSpawnInterval();

    GetWorldTimerManager().SetTimer(SpawnTimerHandle, this, &ALastStandLegacyGameMode::ProcessSpawning, NewInterval, true);
}

void ALastStandLegacyGameMode::RespawnDeadPlayers()
{
    if (!HasAuthority()) return;

    UWorld* World = GetWorld();
    if (!World) return;

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (PC && PC->IsInState(NAME_Spectating))
        {
            RestartPlayer(PC);
        }
    }
}

void ALastStandLegacyGameMode::RestartPlayer(AController* NewPlayer)
{
    if (!NewPlayer || !HasAuthority()) return;

    if (APlayerController* PC = Cast<APlayerController>(NewPlayer))
    {
        PC->ClientGotoState(NAME_Playing);
    }
    NewPlayer->ChangeState(NAME_Playing);

    FTransform TeammateSpawnTransform;

    if (GetSpawnTransformNearTeammate(NewPlayer, TeammateSpawnTransform))
    {
        Super::RestartPlayerAtTransform(NewPlayer, TeammateSpawnTransform);
        UE_LOG(LogTemp, Log, TEXT("[RestartPlayer] Spawned player %s near a teammate."), *NewPlayer->GetName());
        return;
    }

    Super::RestartPlayer(NewPlayer);
}

bool ALastStandLegacyGameMode::GetSpawnTransformNearTeammate(AController* Player, FTransform& OutSpawnTransform)
{
    UWorld* World = GetWorld();
    if (!World) return false;

    TArray<AHama*> AliveTeammates;
    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC || PC == Player) continue;

        AHama* Candidate = Cast<AHama>(PC->GetPawn());
        if (IsValid(Candidate) && !Candidate->bIsDead && !Candidate->IsDowned())
        {
            AliveTeammates.Add(Candidate);
        }
    }

    if (AliveTeammates.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[GM] No alive teammates to spawn near."));
        return false;
    }

    // Shuffle کردنی هاوڕێکان
    for (int32 i = AliveTeammates.Num() - 1; i > 0; --i)
    {
        const int32 j = FMath::RandRange(0, i);
        if (i != j) AliveTeammates.Swap(i, j);
    }

    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

    // تەنها Pawn-ی کۆنی یاریزانە ڕیسپاونبووەکە ignore دەکرێت
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SpawnNearTeammate), false);
    if (Player && Player->GetPawn())
    {
        Params.AddIgnoredActor(Player->GetPawn());
    }

    for (AHama* Teammate : AliveTeammates)
    {
        // ١. وەرگرتنی قەبارەی Capsule لە CDO (ڕاوەستاو) بۆ ئەوەی لە کاتی Crouch/Slide قەبارەکە بچووک نەبێتەوە
        const AHama* DefaultChar = Teammate->GetClass()->GetDefaultObject<AHama>();
        const UCapsuleComponent* DefaultCap = DefaultChar->GetCapsuleComponent();
        const float Radius = DefaultCap->GetScaledCapsuleRadius();
        const float HalfHeight = DefaultCap->GetScaledCapsuleHalfHeight();
        const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);

        // ٢. حیسابکردنی شوێنی پێیەکان لەسەر Capsuleی ئێستای هاوڕێکە
        const float CurrentHalfHeight = Teammate->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const FVector TeammateLoc = Teammate->GetActorLocation();
        const FVector FeetLoc = TeammateLoc - FVector(0.f, 0.f, CurrentHalfHeight);
        const FRotator SpawnRot(0.f, Teammate->GetActorRotation().Yaw, 0.f);

        // A) Navmesh Attempts
        if (NavSys)
        {
            for (int32 Attempt = 0; Attempt < MaxSpawnAttempts; ++Attempt)
            {
                FNavLocation NavLoc;
                if (!NavSys->GetRandomReachablePointInRadius(FeetLoc, TeammateSpawnRadius, NavLoc))
                    continue;

                // پشکنینی جیاوازی نهۆم
                if (FMath::Abs(NavLoc.Location.Z - FeetLoc.Z) > MaxSpawnHeightDiff)
                    continue;

                const FVector Test = NavLoc.Location + FVector(0.f, 0.f, HalfHeight + 2.f);
                if (!World->OverlapBlockingTestByChannel(Test, FQuat::Identity, ECC_Pawn, Shape, Params))
                {
                    OutSpawnTransform = FTransform(SpawnRot, Test);
                    return true;
                }
            }
        }

        FCollisionQueryParams LosParams = Params;
        LosParams.AddIgnoredActor(Teammate);

        for (int32 i = 0; i < 8; ++i)
        {
            const FVector Dir = FRotator(0.f, i * 45.f, 0.f).Vector();
            const FVector Probe = TeammateLoc + Dir * FallbackDistance;

            FHitResult Floor;
            if (!World->LineTraceSingleByChannel(Floor, Probe, Probe - FVector(0.f, 0.f, 300.f), ECC_Visibility, LosParams))
                continue;

            // ڕێگری لە spawnبوون لەسەر Pawn (وەک زۆمبی) یان لەسەر ڕووی زۆر لێژ
            if (Cast<APawn>(Floor.GetActor()) || Floor.ImpactNormal.Z < 0.7f)
                continue;

            const FVector Test = Floor.ImpactPoint + FVector(0.f, 0.f, HalfHeight + 2.f);

            // ڕێگری لە spawnبوون لە پشت دیواری باریک (Line of Sight Check)
            if (World->LineTraceTestByChannel(TeammateLoc, Test, ECC_Visibility, LosParams))
                continue;

            if (!World->OverlapBlockingTestByChannel(Test, FQuat::Identity, ECC_Pawn, Shape, Params))
            {
                OutSpawnTransform = FTransform(SpawnRot, Test);
                return true;
            }
        }
    }

    return false;
}


void ALastStandLegacyGameMode::ProcessSpawning()
{
    if (ZombiesSpawnedThisRound >= ZombiesToKill)
    {
        GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
        return;
    }

    if (ActiveZombiesCount >= ZombiesSpawnLimit)
    {
        return;
    }

    if (!ZombieClass || SpawnPoints.IsEmpty()) return;

    AActor* SpawnPoint = PickWeightedSpawnPoint();
    if (!SpawnPoint) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    AZombie* Zombie = GetWorld()->SpawnActor<AZombie>(
        ZombieClass, SpawnPoint->GetActorLocation(), SpawnPoint->GetActorRotation(), SpawnParams);

    if (Zombie)
    {
        ActiveZombiesCount++;
        ZombiesSpawnedThisRound++;

        Zombie->SetStatsForRound(CurrentRound);
        Zombie->OnZombieDeath.BindUObject(this, &ALastStandLegacyGameMode::HandleZombieDeath);
    }
}

AActor* ALastStandLegacyGameMode::PickWeightedSpawnPoint()
{
    if (SpawnPoints.IsEmpty()) return nullptr;

    TArray<APawn*> Players;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (APlayerController* PC = It->Get())
        {
            if (APawn* Pawn = PC->GetPawn())
            {
                Players.Add(Pawn);
            }
        }
    }

    struct FScoredPoint { AActor* Point; float Weight; };
    TArray<FScoredPoint> ScoredPoints;
    float TotalWeight = 0.f;
    const float MinSafeDistSq = FMath::Square(MinSafeDistance);

    for (AActor* SP : SpawnPoints)
    {
        if (!SP) continue;
        float ClosestDistSq = TNumericLimits<float>::Max();

        for (APawn* Player : Players)
        {
            float DistSq = FVector::DistSquared(SP->GetActorLocation(), Player->GetActorLocation());
            ClosestDistSq = FMath::Min(ClosestDistSq, DistSq);
        }

        if (!Players.IsEmpty() && ClosestDistSq < MinSafeDistSq) continue;

        float Weight = FMath::Sqrt(ClosestDistSq) + 100.f;
        ScoredPoints.Add({ SP, Weight });
        TotalWeight += Weight;
    }

    if (ScoredPoints.IsEmpty())
    {
        return SpawnPoints[FMath::RandRange(0, SpawnPoints.Num() - 1)];
    }

    float RandomValue = FMath::FRandRange(0.f, TotalWeight);
    float CurrentWeight = 0.f;

    for (const FScoredPoint& Entry : ScoredPoints)
    {
        CurrentWeight += Entry.Weight;
        if (RandomValue <= CurrentWeight) return Entry.Point;
    }

    return ScoredPoints.Last().Point;
}

void ALastStandLegacyGameMode::SpawnPowers(FVector SpawnLocation)
{
    if (PowerUpClasses.IsEmpty())
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("ERROR: PowerUp array is empty in GameMode!"));
        return;
    }

    int32 RandomIndex = FMath::RandRange(0, PowerUpClasses.Num() - 1);
    TSubclassOf<ABasePowerUp> SelectedPowerUp = PowerUpClasses[RandomIndex];

    if (SelectedPowerUp)
    {
        FVector AdjustedLocation = SpawnLocation + FVector(0.0f, 0.0f, 40.0f);

        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        GetWorld()->SpawnActor<ABasePowerUp>(SelectedPowerUp, AdjustedLocation, FRotator::ZeroRotator, SpawnParams);

        CurrentPowerSpawn++;

        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, TEXT("Random PowerUp Spawned!"));
    }
}

void ALastStandLegacyGameMode::ActivateNuke()
{
    if (bIsNuking) return;

    bIsNuking = true;
    ZombiesToNuke.Empty();

    for (TActorIterator<AZombie> It(GetWorld()); It; ++It)
    {
        if (*It && !(*It)->IsDead())
        {
            ZombiesToNuke.Add(*It);
        }
    }

    DynamicKillBatchSize = FMath::Max(1, FMath::CeilToInt(ZombiesToNuke.Num() / 10.0f));

    bool bDoublePoints = false;
    if (ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>())
    {
        bDoublePoints = GS->bIsDoublePointsActive;
    }

    int32 NukeReward = bDoublePoints ? 800 : 400;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (APlayerController* PC = It->Get())
        {
            if (AHamaPlayerState* PS = PC->GetPlayerState<AHamaPlayerState>())
            {
                PS->AddPoints(NukeReward);
            }
        }
    }

    GetWorldTimerManager().SetTimer(NukeTimerHandle, this, &ALastStandLegacyGameMode::ProcessNukeKills, 0.05f, true);
}

void ALastStandLegacyGameMode::ProcessNukeKills()
{
    for (int32 i = 0; i < DynamicKillBatchSize; i++)
    {
        if (ZombiesToNuke.IsEmpty())
        {
            bIsNuking = false;
            GetWorldTimerManager().ClearTimer(NukeTimerHandle);
            return;
        }

        AZombie* ZombieToKill = ZombiesToNuke.Pop();

        if (ZombieToKill && !ZombieToKill->IsDead())
        {
            ZombieToKill->Die(nullptr);
        }
    }
}

void ALastStandLegacyGameMode::CheckGameOverCondition()
{
    ALastStandLegacyGameState* GS = GetGameState<ALastStandLegacyGameState>();
    if (!GS) return;

    bool bIsAnyPlayerAliveAndUp = false;

    for (APlayerState* PS : GS->PlayerArray)
    {
        if (!PS) continue;

        AHama* Character = Cast<AHama>(PS->GetPawn());

        if (IsValid(Character) && !Character->bIsDead && !Character->IsDowned())
        {
            bIsAnyPlayerAliveAndUp = true;
            break;
        }
    }

    if (!bIsAnyPlayerAliveAndUp)
    {
        TriggerGameOver();
    }
}

void ALastStandLegacyGameMode::Logout(AController* Exiting)
{
    Super::Logout(Exiting);

    CheckGameOverCondition();
}

void ALastStandLegacyGameMode::TriggerGameOver()
{
}