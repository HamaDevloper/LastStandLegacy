#include "DoublePoint.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/RotatingMovementComponent.h" // ئینکلودی پێویست بۆ خولانەوە
#include "Hama.h"
#include "LastStandLegacyGameState.h"

ADoublePoint::ADoublePoint()
{
    // تیک دەکوژێنینەوە بۆ ئەوەی CPU بەخۆڕایی ماندوو نەبێت
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
    RootComponent = CollisionSphere;
    CollisionSphere->SetSphereRadius(60.f);
    CollisionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(RootComponent);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // دروستکردنی خولانەوەی ئۆپتیمایزکراو (90 پلە لە چرکەیەکدا بە دەوری Z)
    RotatingComp = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RotatingComp"));
    RotatingComp->RotationRate = FRotator(0.f, 90.f, 0.f);
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

// فەنکشنی Tick بەتەواوی سڕاوەتەوە چونکە RotatingComp کارەکە دەکات

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