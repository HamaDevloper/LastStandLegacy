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
        if (PS->GetPoints() >= AmmoCost)
        {
            PS->RemovePoints(AmmoCost);
            HamaChar->RefillSpecificWeaponAmmo(WeaponClass);
        }
    }
    else
    {
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