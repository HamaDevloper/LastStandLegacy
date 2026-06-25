#include "Nuke.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Hama.h"
#include "LastStandLegacyGameMode.h"

ANuke::ANuke()
{
    // کوژاندنەوەی تیک بۆ پێرفۆرمانس
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
    RootComponent = CollisionSphere;
    CollisionSphere->SetSphereRadius(60.f);
    CollisionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(RootComponent);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // خولانەوە
    RotatingComp = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RotatingComp"));
    RotatingComp->RotationRate = FRotator(0.f, 90.f, 0.f);
}

void ANuke::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &ANuke::OnOverlapBegin);
        SetLifeSpan(15.0f);
    }
}

void ANuke::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!HasAuthority()) return;

    if (AHama* Player = Cast<AHama>(OtherActor))
    {
        if (ALastStandLegacyGameMode* GM = GetWorld()->GetAuthGameMode<ALastStandLegacyGameMode>())
        {
            GM->ActivateNuke();
        }

        Destroy();
    }
}