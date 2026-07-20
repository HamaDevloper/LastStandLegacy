#include "BasePowerUp.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Hama.h"

ABasePowerUp::ABasePowerUp()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
    RootComponent = CollisionSphere;
    CollisionSphere->SetSphereRadius(60.f);
    CollisionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    CollisionSphere->SetHiddenInGame(false);
    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(RootComponent);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABasePowerUp::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &ABasePowerUp::OnOverlapBegin);
        SetLifeSpan(15);
    }
}

void ABasePowerUp::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!HasAuthority()) return;

   if (AHama* Player = Cast<AHama>(OtherActor))
    {
        ActivatePowerUp(Player);

        Destroy();
    }
}
void ABasePowerUp::ActivatePowerUp(AHama* Player)
{
}