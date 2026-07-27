#include "MysteryBoxSpawnPoint.h"
#include "Components/SceneComponent.h"
#include "ZombieDirectorSubsystem.h"

AMysteryBoxSpawnPoint::AMysteryBoxSpawnPoint()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;

    SpawnTransformComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SpawnTransformComponent"));
    RootComponent = SpawnTransformComponent;
}

void AMysteryBoxSpawnPoint::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        if (UZombieDirectorSubsystem* Director = GetWorld()->GetSubsystem<UZombieDirectorSubsystem>())
        {
            Director->RegisterMysteryBoxSpawnPoint(this);
        }
    }
}

void AMysteryBoxSpawnPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (HasAuthority())
    {
        if (UZombieDirectorSubsystem* Director = GetWorld()->GetSubsystem<UZombieDirectorSubsystem>())
        {
            Director->UnregisterMysteryBoxSpawnPoint(this);
        }
    }
    Super::EndPlay(EndPlayReason);
}