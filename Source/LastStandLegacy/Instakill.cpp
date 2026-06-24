
#include "Instakill.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Hama.h"
#include "LastStandLegacyGameState.h"

// Sets default values
AInstakill::AInstakill()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
    RootComponent = CollisionSphere;
    CollisionSphere->SetSphereRadius(60.f);
    CollisionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(RootComponent);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

}

// Called when the game starts or when spawned
void AInstakill::BeginPlay()
{
	Super::BeginPlay();

    if (HasAuthority())
    {
        CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &AInstakill::OnOverlapBegin);

        SetLifeSpan(15.0f);
    }
	
}

// Called every frame
void AInstakill::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
    AddActorLocalRotation(FRotator(0.f, 90.f * DeltaTime, 0.f));

}

void AInstakill::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!HasAuthority()) return;

    if (AHama* Player = Cast<AHama>(OtherActor))
    {
        if (ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>())
        {
            GS->StartinstaKill(Duration);
        }
        Destroy();
    }
}