#include "GobbleGumAction_HappyHour.h"
#include "GameFramework/Character.h"
#include "GobbleGumComponent.h"
#include "BasePerk.h"
#include "Hama.h"

UGobbleGumAction_HappyHour::UGobbleGumAction_HappyHour()
{
}

void UGobbleGumAction_HappyHour::ActivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority()) return;
    if (bIsActive) return;

    UGobbleGumComponent* GumComp = CharacterOwner->FindComponentByClass<UGobbleGumComponent>();
    if (!GumComp) return;

    Super::ActivateAction_Implementation(CharacterOwner);

    CachedGumComponent = GumComp;
    GumComp->SetActiveHappyHourAction(this);
}

void UGobbleGumAction_HappyHour::DeactivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (bIsActive)
    {
        UGobbleGumComponent* GumComp = CachedGumComponent.Get();
        if (!GumComp && CharacterOwner)
        {
            GumComp = CharacterOwner->FindComponentByClass<UGobbleGumComponent>();
        }

        if (GumComp)
        {
            GumComp->ClearActiveHappyHourAction(this);
        }
    }

    CachedGumComponent.Reset();
    Super::DeactivateAction_Implementation(CharacterOwner);
}

bool UGobbleGumAction_HappyHour::TryGrantRandomBonusPerk(AHama* Player, FName ExcludedPerkID)
{
    if (!IsValid(Player) || !Player->HasAuthority() || PerkMachineClasses.Num() == 0)
    {
        return false;
    }

    TArray<FName> AvailablePerkIDs;
    AvailablePerkIDs.Reserve(PerkMachineClasses.Num());

    for (const TSubclassOf<ABasePerk>& PerkClass : PerkMachineClasses)
    {
        if (!PerkClass) continue;

        const ABasePerk* CDO = PerkClass.GetDefaultObject();
        if (!CDO || CDO->PerkID.IsNone()) continue;

        // ١. پێرکی ئێستا کڕدراو و پێرکە هەبووەکانی یاریزان دەردەکەین
        if (CDO->PerkID == ExcludedPerkID || Player->HasPerkID(CDO->PerkID))
        {
            continue;
        }

        AvailablePerkIDs.AddUnique(CDO->PerkID);
    }

    // ئەگەر یاریزان هەموو پێرکەکانی هەبوو، false دەگەڕێنینەوە تا گەمەکەی بەفیڕۆ نەچێت
    if (AvailablePerkIDs.Num() == 0)
    {
        return false;
    }

    const int32 RandomIndex = FMath::RandRange(0, AvailablePerkIDs.Num() - 1);
    const FName BonusPerkID = AvailablePerkIDs[RandomIndex];

    // لێرەدا هەمان فەنکشنی پێدانی پێرکی ڕاستەوخۆی AHama بانگ بکە (کە لە Perkaholic و Random Perk بەکارت هێناوە)
    Player->AddPerkByID(BonusPerkID);

    return true;
}