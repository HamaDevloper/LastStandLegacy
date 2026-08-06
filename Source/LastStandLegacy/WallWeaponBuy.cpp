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
    SetReplicatingMovement(false);

    NetDormancy = DORM_Initial;
    SetNetUpdateFrequency(1.f);
    SetMinNetUpdateFrequency(0.5f);

    InteractBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractBox"));
    RootComponent = InteractBox;
    InteractBox->SetMobility(EComponentMobility::Static);
    InteractBox->SetCollisionProfileName(TEXT("Trigger"));
    InteractBox->SetGenerateOverlapEvents(true);
    InteractBox->SetCollisionResponseToChannel(ECC_Intract, ECR_Block);
    InteractBox->PrimaryComponentTick.bCanEverTick = false;

    WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
    WeaponMesh->SetupAttachment(RootComponent);
    WeaponMesh->SetMobility(EComponentMobility::Static);
    WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WeaponMesh->PrimaryComponentTick.bCanEverTick = false;
}

bool AWallWeaponBuy::CanInteract(AHama* InteractingPlayer)
{
    if (!InteractingPlayer || !WeaponClass) return false;

    if (InteractingPlayer->IsDowned() || InteractingPlayer->bIsDeathMachineActive)
    {
        return false;
    }

    // ١. ڕێگریکردن لە کڕینی چەک لە دیوار ئەگەر چەکەکە ئێستا لە Pack-a-Punch بێت
    if (InteractingPlayer->IsWeaponCurrentlyUpgrading(WeaponClass))
    {
        return false;
    }

    ABaseWeapon* OwnedWeapon = InteractingPlayer->GetWeaponOrUpgradedInstance(WeaponClass);

    if (OwnedWeapon)
    {
        return OwnedWeapon->NeedsAmmo();
    }

    return true;
}

void AWallWeaponBuy::Interact(AHama* HamaChar)
{
    if (!HamaChar || !HasAuthority() || !WeaponClass) return;
    if (HamaChar->IsWeaponCurrentlyUpgrading(WeaponClass)) return;

    AHamaPlayerState* PS = HamaChar->GetPlayerState<AHamaPlayerState>();
    if (!PS) return;

    ABaseWeapon* TargetWeaponToRefill = HamaChar->GetWeaponOrUpgradedInstance(WeaponClass);

    if (TargetWeaponToRefill)
    {
        if (TargetWeaponToRefill->NeedsAmmo())
        {
            const bool bIsWeaponUpgraded = (TargetWeaponToRefill->GetClass() != WeaponClass);

            const int32 FinalAmmoCost = bIsWeaponUpgraded ? UpgradedAmmoCost : AmmoCost;

            if (PS->GetPoints() >= FinalAmmoCost)
            {
                PS->RemovePoints(FinalAmmoCost);
                TargetWeaponToRefill->RefillAmmo();

                if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Green, TEXT("AMMO BOUGHT!"));
            }
            else
            {
                if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Red, TEXT("Not enough points for ammo!"));
            }
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
    if (!WeaponClass) return FString();

    return FString::Printf(TEXT("Hold [E] Buy Weapon [%d Points]"), WeaponCost);
}