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

void AWallWeaponBuy::Interact(AHama* HamaChar)
{
    if (!HamaChar || !HasAuthority() || !WeaponClass) return;

    AHamaPlayerState* PS = HamaChar->GetPlayerState<AHamaPlayerState>();
    if (!PS) return;

    ABaseWeapon* TargetWeaponToRefill = HamaChar->GetWeaponByClass(WeaponClass);

    if (TargetWeaponToRefill)
    {
        if (TargetWeaponToRefill->NeedsAmmo())
        {
            if (PS->GetPoints() >= AmmoCost)
            {
                PS->RemovePoints(AmmoCost);
                HamaChar->RefillSpecificWeaponAmmo(WeaponClass);

                if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Green, TEXT("AMMO BOUGHT!"));
            }
            else
            {
                if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Red, TEXT("Not enough points!"));
            }
        }
        else
        {
            if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Yellow, TEXT("Ammo is already full!"));
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

bool AWallWeaponBuy::CanInteract(AHama* InteractingPlayer)
{
    if (!InteractingPlayer || !WeaponClass) return false;
    
    if (InteractingPlayer->IsDowned() || InteractingPlayer->bIsDeathMachineActive)
    {
        return false;
    }

    ABaseWeapon* OwnedWeapon = InteractingPlayer->GetWeaponByClass(WeaponClass);

    if (OwnedWeapon)
    {
        return OwnedWeapon->NeedsAmmo();
    }

    return true;
}