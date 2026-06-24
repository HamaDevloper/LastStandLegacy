#include "DoublePoint.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Hama.h"
#include "LastStandLegacyGameState.h"

ADoublePoint::ADoublePoint()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
 
    CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
    RootComponent = CollisionSphere;
    CollisionSphere->SetSphereRadius(60.f);
    CollisionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(RootComponent);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision); // مێشەکە پێویستی بە کۆلیژن نییە
}

void ADoublePoint::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &ADoublePoint::OnOverlapBegin);

        SetLifeSpan(15.0f);
    }
}

void ADoublePoint::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    AddActorLocalRotation(FRotator(0.f, 90.f * DeltaTime, 0.f));
}

void ADoublePoint::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!HasAuthority()) return;

    if (AHama* Player = Cast<AHama>(OtherActor))
    {
        if (ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>())
        {
            GS->StartDoublePoints(Duration);
        }
        Destroy();
    }
}