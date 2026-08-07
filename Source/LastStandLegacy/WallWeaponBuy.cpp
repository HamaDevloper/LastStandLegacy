#include "WallWeaponBuy.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Hama.h"
#include "HamaPlayerState.h"
#include "BaseWeapon.h"
#include "Kismet/GameplayStatics.h"

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
    if (!IsValid(InteractingPlayer) || !WeaponClass) return false;

    if (InteractingPlayer->IsDowned() || InteractingPlayer->bIsDeathMachineActive || InteractingPlayer->IsDrinkingPerk())
    {
        return false;
    }

    return true;
}

bool AWallWeaponBuy::Client_PreInteract(AHama* InteractingPlayer)
{
    if (!IsValid(InteractingPlayer) || !WeaponClass) return false;

    if (InteractingPlayer->IsDowned() || InteractingPlayer->bIsDeathMachineActive)
    {
        return false;
    }

    if (InteractingPlayer->IsWeaponCurrentlyUpgrading(WeaponClass))
    {
        return false;
    }

    AHamaPlayerState* PS = InteractingPlayer->GetPlayerState<AHamaPlayerState>();
    if (!PS) return false;

    ABaseWeapon* OwnedWeapon = InteractingPlayer->GetWeaponOrUpgradedInstance(WeaponClass);
    int32 RequiredCost = WeaponCost;

    if (OwnedWeapon)
    {
        if (!OwnedWeapon->NeedsAmmo())
        {
            return false;
        }

        const bool bIsWeaponUpgraded = (OwnedWeapon->GetClass() != WeaponClass);
        RequiredCost = bIsWeaponUpgraded ? UpgradedAmmoCost : AmmoCost;
    }

    if (PS->GetPoints() < RequiredCost)
    {
        if (RejectSound && InteractingPlayer->IsLocallyControlled())
        {
            UGameplayStatics::PlaySound2D(this, RejectSound);
        }

        return false;
    }

    return true;
}

void AWallWeaponBuy::Interact(AHama* InteractingPlayer)
{
    if (!HasAuthority() || !IsValid(InteractingPlayer) || !WeaponClass) return;
    if (!CanInteract(InteractingPlayer)) return;
    if (InteractingPlayer->IsWeaponCurrentlyUpgrading(WeaponClass)) return;

    AHamaPlayerState* PS = InteractingPlayer->GetPlayerState<AHamaPlayerState>();
    if (!PS) return;

    ABaseWeapon* TargetWeaponToRefill = InteractingPlayer->GetWeaponOrUpgradedInstance(WeaponClass);

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
            }
        }
    }
    else
    {
        if (PS->GetPoints() >= WeaponCost)
        {
            PS->RemovePoints(WeaponCost);
            InteractingPlayer->GiveWeapon(WeaponClass);
        }
    }
}

FString AWallWeaponBuy::GetInteractMessage(AHama* InteractingPlayer)
{
    if (!WeaponClass) return FString();

    if (InteractingPlayer)
    {

        if (InteractingPlayer->IsWeaponCurrentlyUpgrading(WeaponClass))
        {
            return TEXT("Weapon Upgrading in Pack-a-Punch...");
        }

        ABaseWeapon* OwnedWeapon = InteractingPlayer->GetWeaponOrUpgradedInstance(WeaponClass);
        if (OwnedWeapon)
        {
            if (!OwnedWeapon->NeedsAmmo())
            {
                return TEXT("Ammo Full");
            }

            const bool bIsWeaponUpgraded = (OwnedWeapon->GetClass() != WeaponClass);
            const int32 DisplayCost = bIsWeaponUpgraded ? UpgradedAmmoCost : AmmoCost;

            return FString::Printf(TEXT("Hold [E] Buy Ammo [%d Points]"), DisplayCost);
        }
    }

    return FString::Printf(TEXT("Hold [E] Buy Weapon [%d Points]"), WeaponCost);
}