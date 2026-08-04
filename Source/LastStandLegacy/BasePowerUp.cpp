#include "BasePowerUp.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "LastStandLegacyGameState.h"
#include "Hama.h"

ABasePowerUp::ABasePowerUp()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicatingMovement(false);

    NetDormancy = DORM_Initial;

    SetNetUpdateFrequency(1.f);
    SetMinNetUpdateFrequency(0.5f);

    CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
    RootComponent = CollisionSphere;
    CollisionSphere->SetSphereRadius(60.f);
    CollisionSphere->SetMobility(EComponentMobility::Static);
    CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CollisionSphere->SetCollisionObjectType(ECC_WorldDynamic);
    CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    CollisionSphere->PrimaryComponentTick.bCanEverTick = false;;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(RootComponent);
    MeshComp->SetMobility(EComponentMobility::Static);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComp->PrimaryComponentTick.bCanEverTick = false;
}

void ABasePowerUp::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &ABasePowerUp::OnOverlapBegin);
        SetLifeSpan(15.0f);
    }
}

void ABasePowerUp::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (!HasAuthority() || bIsConsumed) return;

    AHama* Player = Cast<AHama>(OtherActor);
    if (!Player) return;

    bIsConsumed = true;
    CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ActivatePowerUp(Player);

    if (ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>())
    {
        GS->Multicast_AnnouncePowerUp(PowerUpType);
    }

    Destroy();
}

void ABasePowerUp::ActivatePowerUp(AHama* Player)
{
}