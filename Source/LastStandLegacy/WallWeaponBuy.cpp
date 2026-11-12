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

    SetNetDormancy(DORM_Initial);
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

#define LOCTEXT_NAMESPACE "WallWeaponBuy"

int32 AWallWeaponBuy::GetCurrentPrice(const AHama* Player) const
{
    if (Player && Player->IsShoppingFreeActive())
    {
        return 0;
    }
    ABaseWeapon* OwnedWeapon = Player ? Player->GetWeaponByClass(WeaponClass) : nullptr;
    if (OwnedWeapon)
    {
        const bool bIsWeaponUpgraded = (OwnedWeapon->GetClass() != WeaponClass);
        return bIsWeaponUpgraded ? UpgradedAmmoCost : AmmoCost;
    }
    return WeaponCost;
}

bool AWallWeaponBuy::CanInteract(AHama* InteractingPlayer)
{
    if (!IsValid(InteractingPlayer) || !WeaponClass) return false;

    return InteractingPlayer->SetCanInteract();
}

bool AWallWeaponBuy::Client_PreInteract(AHama* InteractingPlayer)
{
    if (!IsValid(InteractingPlayer) || !WeaponClass) return false;

    if(!CanInteract(InteractingPlayer))
    {
        return false;
    }

    if (InteractingPlayer->IsWeaponCurrentlyUpgrading(WeaponClass))
    {
        return false;
    }

    float DistanceSq = FVector::DistSquared(InteractingPlayer->GetActorLocation(), GetActorLocation());
    if (DistanceSq > FMath::Square(250.f))   return false;

    AHamaPlayerState* PS = InteractingPlayer->GetPlayerState<AHamaPlayerState>();
    if (!PS) return false;

    int32 Cost = GetCurrentPrice(InteractingPlayer);
    ABaseWeapon* OwnedWeapon = InteractingPlayer->GetWeaponOrUpgradedInstance(WeaponClass);

    if (OwnedWeapon)
    {
        if (!OwnedWeapon->NeedsAmmo())
        {
            return false;
        }
    }

    if (PS->GetPoints() < Cost)
    {
        if (RejectSound && InteractingPlayer->IsLocallyControlled())
        {
            UGameplayStatics::PlaySound2D(this, RejectSound);
        }

        return false;
    }

    if (InteractingPlayer->bIsFireButtonHold && InteractingPlayer->CurrentWeapon)
    {
        InteractingPlayer->CurrentWeapon->StopFire();
    }

    return true;
}

void AWallWeaponBuy::Interact(AHama* InteractingPlayer)
{
    if (!HasAuthority() || !IsValid(InteractingPlayer) || !WeaponClass) return;
    if (!CanInteract(InteractingPlayer)) return;
    if (InteractingPlayer->IsWeaponCurrentlyUpgrading(WeaponClass)) return;

    constexpr float MaxDistSq = 250.0f * 250.0f;
    const float DistanceSq = FVector::DistSquared(InteractingPlayer->GetActorLocation(), GetActorLocation());
    if (DistanceSq > MaxDistSq) return;

    AHamaPlayerState* PS = InteractingPlayer->GetPlayerState<AHamaPlayerState>();
    if (!PS) return;

    const int32 Cost = GetCurrentPrice(InteractingPlayer);
    ABaseWeapon* TargetWeaponToRefill = InteractingPlayer->GetWeaponOrUpgradedInstance(WeaponClass);

    if (TargetWeaponToRefill)
    {
        if (TargetWeaponToRefill->NeedsAmmo())
        {
            if (PS->GetPoints() >= Cost)
            {
                if (Cost > 0)
                {
                    PS->RemovePoints(Cost);
                }
                TargetWeaponToRefill->RefillAmmo();
            }
        }
    }
    else
    {
        if (PS->GetPoints() >= Cost)
        {
            if (Cost > 0)
            {
                PS->RemovePoints(Cost);
            }
            InteractingPlayer->GiveWeapon(WeaponClass);
        }
    }
}

FText AWallWeaponBuy::GetInteractMessage(AHama* InteractingPlayer)
{
    if (!WeaponClass || !IsValid(InteractingPlayer))
    {
        return FText::GetEmpty();
    }

    if (InteractingPlayer->IsWeaponCurrentlyUpgrading(WeaponClass))
    {
        return LOCTEXT("WeaponUpgrading", "Weapon Upgrading in Pack-a-Punch...");
    }

    ABaseWeapon* OwnedWeapon = InteractingPlayer->GetWeaponOrUpgradedInstance(WeaponClass);
    if (OwnedWeapon && !OwnedWeapon->NeedsAmmo())
    {
        return FText::GetEmpty();
    }

    const int32 Cost = GetCurrentPrice(InteractingPlayer);

    if (OwnedWeapon)
    {
        if (Cost <= 0)
        {
            return LOCTEXT("BuyAmmoFree", "Hold [F] to Buy Ammo [FREE]");
        }

        FFormatNamedArguments Args;
        Args.Add(TEXT("Cost"), FText::FromString(FString::FromInt(Cost)));
        return FText::Format(LOCTEXT("BuyAmmoWithCost", "Hold [F] to Buy Ammo [Cost: {Cost}]"), Args);
    }

    if (Cost <= 0)
    {
        return LOCTEXT("BuyWeaponFree", "Hold [F] to Buy Weapon [FREE]");
    }

    FFormatNamedArguments Args;
    Args.Add(TEXT("Cost"), FText::FromString(FString::FromInt(Cost)));
    return FText::Format(LOCTEXT("BuyWeaponWithCost", "Hold [F] to Buy Weapon [Cost: {Cost}]"), Args);
}

#undef LOCTEXT_NAMESPACE