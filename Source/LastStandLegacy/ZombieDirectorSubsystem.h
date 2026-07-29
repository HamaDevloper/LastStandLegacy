#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SpatialHashGrid2D.h"
#include "ZombieDirectorSubsystem.generated.h"

class AZombie;
class APawn;
class AMysteryBoxSpawnPoint;


UCLASS()
class LASTSTANDLEGACY_API UZombieDirectorSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override { return !IsTemplate(); }

    void RegisterZombie(AZombie* Zombie);
    void UnregisterZombie(AZombie* Zombie);

    void IsPlayerTargetable(APawn* PlayerPawn) const;
    void RegisterPlayer(APawn* Player);
    void UnregisterPlayer(APawn* Player);

    void SetPlayerTargetable(APawn* Player, bool bIsTargetable);

    void RegisterMysteryBoxSpawnPoint(AMysteryBoxSpawnPoint* Point);
    void UnregisterMysteryBoxSpawnPoint(AMysteryBoxSpawnPoint* Point);
    AMysteryBoxSpawnPoint* GetRandomFreeMysteryBoxPoint(AMysteryBoxSpawnPoint* CurrentPoint);

private:
    UPROPERTY()
    TArray<TWeakObjectPtr<APawn>> ActivePlayers;

    UPROPERTY()
    TArray<TWeakObjectPtr<APawn>> ValidTargetPlayers;

    UPROPERTY()
    TArray<TWeakObjectPtr<AZombie>> ActiveZombies;

    TMap<APawn*, FVector> CachedPlayerMap;
    float PlayerCacheRefreshTimer = 0.f;

    UPROPERTY()
    TArray<TObjectPtr<AMysteryBoxSpawnPoint>> MysteryBoxSpawnPoints;

    UPROPERTY()
    TArray<AMysteryBoxSpawnPoint*> ReusableMysteryBoxPoints;

    FSpatialHashGrid2D SpatialGrid;

    int32 CurrentZombieIndex = 0;
};