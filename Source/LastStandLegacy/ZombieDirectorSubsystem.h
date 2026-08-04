#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SpatialHashGrid2D.h"
#include "ZombieDirectorSubsystem.generated.h"

class AZombie;
class APawn;
class AMysteryBoxSpawnPoint;
class AMysteryBox;

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

    // --- Zombie Management ---
    void RegisterZombie(AZombie* Zombie);
    void UnregisterZombie(AZombie* Zombie);

    // --- Player Management ---
    void RegisterPlayer(APawn* Player);
    void UnregisterPlayer(APawn* Player);
    void SetPlayerTargetable(APawn* Player, bool bIsTargetable);

    // --- MysteryBox & FireSale API ---
    void RegisterMysteryBoxSpawnPoint(AMysteryBoxSpawnPoint* Point);
    void UnregisterMysteryBoxSpawnPoint(AMysteryBoxSpawnPoint* Point);
    AMysteryBoxSpawnPoint* GetRandomFreeMysteryBoxPoint(AMysteryBoxSpawnPoint* CurrentPoint);

    void RegisterMysteryBox(AMysteryBox* Box);
    void UnregisterMysteryBox(AMysteryBox* Box);

    void StartFireSale(float Duration);
    void EndFireSale();
    bool IsFireSaleActive() const { return bIsFireSaleActive; }

    UPROPERTY(EditDefaultsOnly, Category = "Mystery Box")
    TSubclassOf<AMysteryBox> MysteryBoxClass;

private:
    UPROPERTY()
    TArray<TWeakObjectPtr<APawn>> ActivePlayers;

    UPROPERTY()
    TArray<TWeakObjectPtr<APawn>> ValidTargetPlayers;

    UPROPERTY()
    TArray<TWeakObjectPtr<AZombie>> ActiveZombies;

    TMap<TWeakObjectPtr<APawn>, FVector> CachedPlayerMap;
    float PlayerCacheRefreshTimer = 0.f;

    UPROPERTY()
    TArray<TObjectPtr<AMysteryBoxSpawnPoint>> MysteryBoxSpawnPoints;

    UPROPERTY()
    TArray<TObjectPtr<AMysteryBoxSpawnPoint>> ReusableMysteryBoxPoints;

    UPROPERTY()
    TArray<TObjectPtr<AMysteryBox>> RegisteredBoxes;

    UPROPERTY()
    TArray<TObjectPtr<AMysteryBox>> TempFireSaleBoxes;

    FTimerHandle TimerHandle_FireSale;
    bool bIsFireSaleActive = false;

    FSpatialHashGrid2D SpatialGrid;

    int32 CurrentZombieIndex = 0;
};