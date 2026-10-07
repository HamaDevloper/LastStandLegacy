#include "GrenadeAmmo.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Hama.h"
#include "HamaPlayerState.h"
#include "ThrowableComponent.h"
#include "Kismet/GameplayStatics.h"

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
    CollisionComp->SetGenerateOverlapEvents(true);
    CollisionComp->PrimaryComponentTick.bCanEverTick = false;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(RootComponent);
    MeshComp->SetMobility(EComponentMobility::Static);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComp->PrimaryComponentTick.bCanEverTick = false;
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
    if (!CanInteract(InteractingPlayer)) return false;

    UThrowableComponent* ThrowableComp = InteractingPlayer->FindComponentByClass<UThrowableComponent>();
    if (!ThrowableComp || ThrowableComp->GetCurrentGrenadeCount() >= ThrowableComp->GetMaxGrenadeCount())
    {
        return false;
    }

    AHamaPlayerState* PS = InteractingPlayer->GetPlayerState<AHamaPlayerState>();
    if (!PS) return false;

    if (PS->GetPoints() < GrenadeCost)
    {
        if (RejectSound && InteractingPlayer->IsLocallyControlled())
        {
            UGameplayStatics::PlaySound2D(this, RejectSound);
        }
        return false;
    }

    float DistanceSq = FVector::DistSquared(InteractingPlayer->GetActorLocation(), GetActorLocation());
    if (DistanceSq > FMath::Square(250.f)) return false;

    return true;
}

void AGrenadeAmmo::Interact(AHama* InteractingPlayer)
{
    if (!HasAuthority() || !IsValid(InteractingPlayer)) return;
    if (!CanInteract(InteractingPlayer)) return;

    float DistanceSq = FVector::DistSquared(InteractingPlayer->GetActorLocation(), GetActorLocation());
    if (DistanceSq > FMath::Square(250.f)) return;

    UThrowableComponent* ThrowableComp = InteractingPlayer->FindComponentByClass<UThrowableComponent>();
    if (!ThrowableComp) return;

    if (ThrowableComp->GetCurrentGrenadeCount() >= ThrowableComp->GetMaxGrenadeCount()) return;

    AHamaPlayerState* PS = InteractingPlayer->GetPlayerState<AHamaPlayerState>();
    if (!PS) return;

    if (PS->GetPoints() < GrenadeCost) return;

    PS->RemovePoints(GrenadeCost);
    ThrowableComp->RefillGrenadesToMax();

    if (PurchaseSound)
    {
        UGameplayStatics::PlaySound2D(this, PurchaseSound);
    }
}

FString AGrenadeAmmo::GetInteractMessage(AHama* InteractingPlayer)
{
    return FString::Printf(TEXT("Press F to purchase a grenade for %d points"), GrenadeCost);
}