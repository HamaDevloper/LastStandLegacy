#include "BonusPoints.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Hama.h"
#include "HamaPlayerState.h"
#include "GameFramework/RotatingMovementComponent.h"

// Sets default values
ABonusPoints::ABonusPoints()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
    RootComponent = CollisionSphere;
    CollisionSphere->SetSphereRadius(60.f);
    CollisionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(RootComponent);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision); // مێشەکە پێویستی بە کۆلیژن نییە

    RotatingComp = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RotatingComp"));
    RotatingComp->RotationRate = FRotator(0.f, 90.f, 0.f);
}

// Called when the game starts or when spawned
void ABonusPoints::BeginPlay()
{
	Super::BeginPlay();
    if (HasAuthority())
    {
        CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &ABonusPoints::OnOverlapBegin);

        SetLifeSpan(15.0f);
    }
}

void ABonusPoints::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!HasAuthority()) return;

    if (AHama* Hama = Cast<AHama>(OtherActor))
    {
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            if (APlayerController* PC = It->Get())
            {
                if (AHamaPlayerState* PS = PC->GetPlayerState<AHamaPlayerState>())
                {
                    PS->AddPoints(AddPoints);
                }
            }
        }
        Destroy();
    }
}