#include "UGobbleGumAction_Perkaholic.h"
#include "Hama.h"
#include "BasePerk.h"
#include "EngineUtils.h"

UGobbleGumAction_Perkaholic::UGobbleGumAction_Perkaholic()
{
}

void UGobbleGumAction_Perkaholic::ActivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority()) return;

    Super::ActivateAction_Implementation(CharacterOwner);

    AHama* HamaChar = Cast<AHama>(CharacterOwner);
    UWorld* World = CharacterOwner->GetWorld();
    if (!HamaChar || !World)
    {
        NotifyActionFinished(CharacterOwner);
        return;
    }

    // حاڵەتی ١: ئەگەر لەناو DataAsset ناوی پێرکەکان بە دەست دیاری کرابوون
    if (CustomPerksToGrant.Num() > 0)
    {
        for (const FName& TargetPerkID : CustomPerksToGrant)
        {
            if (!TargetPerkID.IsNone() && !HamaChar->HasPerkID(TargetPerkID))
            {
                HamaChar->AddPerkByID(TargetPerkID);
            }
        }
    }
    // حاڵەتی ٢: دۆزینەوەی هەموو ئامێرەکانی پێرک لەناو مەپەکەدا
    else
    {
        for (TActorIterator<ABasePerk> It(World); It; ++It)
        {
            ABasePerk* PerkMachine = *It;
            if (IsValid(PerkMachine) && !PerkMachine->IsPerkDepleted())
            {
                const FName MapPerkID = PerkMachine->GetPerkID();
                if (!MapPerkID.IsNone() && !HamaChar->HasPerkID(MapPerkID))
                {
                    HamaChar->AddPerkByID(MapPerkID);
                }
            }
        }
    }

    NotifyActionFinished(CharacterOwner);
}