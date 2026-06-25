// Fill out your copyright notice in the Description page of Project Settings.


#include "MaxAmmo.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Hama.h"
#include "HamaAbilityComponent.h"

// Sets default values
AMaxAmmo::AMaxAmmo()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
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

// Called when the game starts or when spawned
void AMaxAmmo::BeginPlay()
{
	Super::BeginPlay();

    if (HasAuthority())
    {
        CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &AMaxAmmo::OnOverlapBegin);
        SetLifeSpan(15.0f);
    }
}

void AMaxAmmo::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!HasAuthority()) return;

    if (AHama* Hama = Cast<AHama>(OtherActor))
    {
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            if (APlayerController* PC = It->Get())
            {
                if (AHama* PlayerPawn = Cast<AHama>(PC->GetPawn()))
                {
                    PlayerPawn->RefillAllWeapons();
                }
            }
        }
        Destroy();
    }
}