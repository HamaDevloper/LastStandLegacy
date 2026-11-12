#include "GrenadeAmmo.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Hama.h"
#include "HamaPlayerState.h"
#include "ThrowableComponent.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "GrenadeAmmo"

AGrenadeAmmo::AGrenadeAmmo()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    SetNetDormancy(DORM_Initial);
    SetNetUpdateFrequency(1.f);
    SetMinNetUpdateFrequency(0.5f);

    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("InteractBox"));
    RootComponent = CollisionComp;
    CollisionComp->SetMobility(EComponentMobility::Static);
    CollisionComp->SetCollisionProfileName(TEXT("Trigger"));
    CollisionComp->SetCollisionResponseToChannel(ECC_Intract, ECR_Block);
    CollisionComp->SetGenerateOverlapEvents(false); // پێویستمان بە Overlap نییە چونکە Trace بەکاردێت
    CollisionComp->PrimaryComponentTick.bCanEverTick = false;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(RootComponent);
    MeshComp->SetMobility(EComponentMobility::Static);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComp->PrimaryComponentTick.bCanEverTick = false;
}

int32 AGrenadeAmmo::GetCurrentCost(const AHama* Player) const
{
    if (Player && Player->IsShoppingFreeActive())
    {
        return 0;
    }

    return GrenadeCost;
}

bool AGrenadeAmmo::CanInteract(AHama* InteractingPlayer)
{
    if (!IsValid(InteractingPlayer)) return false;
    if (!InteractingPlayer->SetCanInteract()) return false;

    UThrowableComponent* ThrowableComp = InteractingPlayer->GetThrowableComponent();
    if (!ThrowableComp) return false;

    if (ThrowableComp->GetCurrentGrenadeCount() >= ThrowableComp->GetMaxGrenadeCount())
    {
        return false;
    }

    return true;
}

bool AGrenadeAmmo::Client_PreInteract(AHama* InteractingPlayer)
{
    if (!InteractingPlayer || !CanInteract(InteractingPlayer)) return false;

    constexpr float MaxDistSq = 250.f * 250.f;
    const float DistanceSq = FVector::DistSquared(InteractingPlayer->GetActorLocation(), GetActorLocation());
    if (DistanceSq > MaxDistSq) return false;

    AHamaPlayerState* PS = InteractingPlayer->GetPlayerState<AHamaPlayerState>();
    if (!PS) return false;

    const int32 Cost = GetCurrentCost(InteractingPlayer);

    if (PS->GetPoints() < Cost)
    {
        if (RejectSound && InteractingPlayer->IsLocallyControlled())
        {
            UGameplayStatics::PlaySound2D(this, RejectSound);
        }
        return false;
    }

    if (PurchaseSound && InteractingPlayer->IsLocallyControlled())
    {
        UGameplayStatics::PlaySound2D(this, PurchaseSound);
    }

    return true;
}

void AGrenadeAmmo::Interact(AHama* InteractingPlayer)
{
    if (!HasAuthority() || !CanInteract(InteractingPlayer)) return;

    constexpr float MaxDistSq = 250.f * 250.f;
    const float DistanceSq = FVector::DistSquared(InteractingPlayer->GetActorLocation(), GetActorLocation());
    if (DistanceSq > MaxDistSq) return;

    UThrowableComponent* ThrowableComp = InteractingPlayer->GetThrowableComponent();
    if (!ThrowableComp) return;

    AHamaPlayerState* PS = InteractingPlayer->GetPlayerState<AHamaPlayerState>();
    if (!PS) return;

    const int32 Cost = GetCurrentCost(InteractingPlayer);
    if (PS->GetPoints() < Cost) return;

    if (Cost > 0)
    {
        PS->RemovePoints(Cost);
    }

    ThrowableComp->RefillGrenadesToMax();
}

FText AGrenadeAmmo::GetInteractMessage(AHama* InteractingPlayer)
{
    if (!IsValid(InteractingPlayer)) return FText::GetEmpty();

    if (UThrowableComponent* ThrowableComp = InteractingPlayer->GetThrowableComponent())
    {
        if (ThrowableComp->GetCurrentGrenadeCount() >= ThrowableComp->GetMaxGrenadeCount())
        {
            return FText::GetEmpty();
        }
    }

    const int32 Cost = GetCurrentCost(InteractingPlayer);

    if (Cost <= 0)
    {
        return LOCTEXT("PurchaseGrenadeFree", "Press [F] to Buy Grenades [FREE]");
    }

    FFormatNamedArguments Args;
    Args.Add(TEXT("Cost"), FText::FromString(FString::FromInt(Cost)));

    return FText::Format(LOCTEXT("PurchaseGrenadeWithCost", "Press [F] to Buy Grenades [Cost: {Cost}]"), Args);
}

#undef LOCTEXT_NAMESPACE