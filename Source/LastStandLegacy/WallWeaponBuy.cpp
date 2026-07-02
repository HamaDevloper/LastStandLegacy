#include "WallWeaponBuy.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Hama.h"
#include "HamaPlayerState.h"
#include "BaseWeapon.h"

AWallWeaponBuy::AWallWeaponBuy()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    InteractBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractBox"));
    InteractBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    RootComponent = InteractBox;

    WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
    WeaponMesh->SetupAttachment(RootComponent);
    WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // مەشەکە پێویست بە کۆلیژن ناکات
}

void AWallWeaponBuy::Interact(AHama* HamaChar)
{
    if (!HamaChar || !HasAuthority() || !WeaponClass) return;

    AHamaPlayerState* PS = HamaChar->GetPlayerState<AHamaPlayerState>();
    if (!PS) return;

    bool bHasWeapon = HamaChar->HasWeaponClass(WeaponClass);

    if (bHasWeapon)
    {
        // دۆزینەوەی ئەو چەکەی کە هەیەتی بۆ ئەوەی بزانین فیشەکی پێویستە
        ABaseWeapon* TempWeapon = WeaponClass.GetDefaultObject();
        if (!TempWeapon) return;
        FName RowNameToFind = TempWeapon->GetWeaponRowName();

        ABaseWeapon* TargetWeaponToRefill = nullptr;
        if (HamaChar->PrimaryWeapon && HamaChar->PrimaryWeapon->GetWeaponRowName() == RowNameToFind) TargetWeaponToRefill = HamaChar->PrimaryWeapon;
        else if (HamaChar->SecondaryWeapon && HamaChar->SecondaryWeapon->GetWeaponRowName() == RowNameToFind) TargetWeaponToRefill = HamaChar->SecondaryWeapon;
        else if (HamaChar->ThirdWeapon && HamaChar->ThirdWeapon->GetWeaponRowName() == RowNameToFind) TargetWeaponToRefill = HamaChar->ThirdWeapon;

        if (TargetWeaponToRefill && TargetWeaponToRefill->NeedsAmmo())
        {
            if (PS->GetPoints() >= AmmoCost)
            {
                PS->RemovePoints(AmmoCost);
                HamaChar->RefillSpecificWeaponAmmo(WeaponClass);
            }
        }
        else
        {
            // فیشەکی پڕە، هیچ مەکە (دەتوانیت نامەیەکی بۆ دەربکەیت ئەگەر بتەوێت)
            if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, TEXT("Ammo is already full!"));
        }
    }
    else
    {
        // ئەگەر چەکەکەی نەبوو، چەکەکەی پێ دەفرۆشێت
        if (PS->GetPoints() >= WeaponCost)
        {
            PS->RemovePoints(WeaponCost);
            HamaChar->GiveWeapon(WeaponClass);
        }
    }
}

FString AWallWeaponBuy::GetInteractMessage()
{
    return FString::Printf(TEXT("Press F to Buy %s [Cost: %d] / Ammo [Cost: %d]"), *WeaponName, WeaponCost, AmmoCost);
}