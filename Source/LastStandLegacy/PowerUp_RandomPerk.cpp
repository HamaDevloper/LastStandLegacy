#include "PowerUp_RandomPerk.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "BasePerk.h"
#include "Hama.h"

APowerUp_RandomPerk::APowerUp_RandomPerk()
{
    bIsGlobal = true;
}

void APowerUp_RandomPerk::BeginPlay()
{
    bIsGlobal = bGiveToAllPlayers;
    Super::BeginPlay();
}

bool APowerUp_RandomPerk::CanBeCollectedBy(AHama* Player) const
{
    if (!HasAuthority() || PerkMachineClasses.IsEmpty()) return false;

    if (bGiveToAllPlayers)
    {
        if (const UWorld* World = GetWorld())
        {
            if (const AGameStateBase* GS = World->GetGameState())
            {
                for (const APlayerState* PS : GS->PlayerArray)
                {
                    if (const AHama* HamaPlayer = PS ? Cast<AHama>(PS->GetPawn()) : nullptr)
                    {
                        if (PlayerHasAnyUnownedPerk(HamaPlayer))
                        {
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }

    return PlayerHasAnyUnownedPerk(Player);
}

void APowerUp_RandomPerk::ActivatePowerUp(AHama* Player)
{
    Super::ActivatePowerUp(Player);

    if (!HasAuthority() || PerkMachineClasses.IsEmpty()) return;

    if (bGiveToAllPlayers)
    {
        if (UWorld* World = GetWorld())
        {
            if (AGameStateBase* GS = World->GetGameState())
            {
                for (APlayerState* PS : GS->PlayerArray)
                {
                    if (AHama* HamaPlayer = PS ? Cast<AHama>(PS->GetPawn()) : nullptr)
                    {
                        GrantRandomUnownedPerkToPlayer(HamaPlayer);
                    }
                }
            }
        }
    }
    else if (Player)
    {
        GrantRandomUnownedPerkToPlayer(Player);
    }
}

bool APowerUp_RandomPerk::PlayerHasAnyUnownedPerk(const AHama* TargetPlayer) const
{
    if (!IsValid(TargetPlayer) || TargetPlayer->bIsDead || TargetPlayer->IsDowned())
    {
        return false;
    }

    const FName CurrentlyDrinkingID = TargetPlayer->GetPendingPerkID();

    for (const TSubclassOf<ABasePerk>& PerkClass : PerkMachineClasses)
    {
        if (!PerkClass) continue;

        const ABasePerk* DefaultPerk = PerkClass->GetDefaultObject<ABasePerk>();
        if (!DefaultPerk) continue;

        const FName CandidateID = DefaultPerk->GetPerkID();
        if (CandidateID.IsNone()) continue;

        if (!TargetPlayer->HasPerkID(CandidateID) && CandidateID != CurrentlyDrinkingID)
        {
            return true;
        }
    }

    return false;
}

void APowerUp_RandomPerk::GrantRandomUnownedPerkToPlayer(AHama* TargetPlayer) const
{
    if (!IsValid(TargetPlayer) || TargetPlayer->bIsDead || TargetPlayer->IsDowned()) return;

    TArray<FName> CandidatePerkIDs;
    CandidatePerkIDs.Reserve(PerkMachineClasses.Num());

    const FName CurrentlyDrinkingID = TargetPlayer->GetPendingPerkID();

    for (const TSubclassOf<ABasePerk>& PerkClass : PerkMachineClasses)
    {
        if (!PerkClass) continue;

        const ABasePerk* DefaultPerk = PerkClass->GetDefaultObject<ABasePerk>();
        if (!DefaultPerk) continue;

        const FName CandidateID = DefaultPerk->GetPerkID();
        if (CandidateID.IsNone()) continue;

        if (TargetPlayer->HasPerkID(CandidateID) || CandidateID == CurrentlyDrinkingID)
        {
            continue;
        }

        CandidatePerkIDs.AddUnique(CandidateID);
    }

    if (CandidatePerkIDs.IsEmpty()) return;

    const int32 RandomIndex = FMath::RandRange(0, CandidatePerkIDs.Num() - 1);
    TargetPlayer->GrantPerkDirect(CandidatePerkIDs[RandomIndex]);
}