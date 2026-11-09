#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SpatialHashGrid2D.h"
#include "ZombieDirectorSubsystem.generated.h"

class AZombie;
class APawn;
class AMysteryBoxSpawnPoint;
class AMysteryBox;
class AZombieSpawnPoint;

struct FCachedTargetInfo
{
    TWeakObjectPtr<APawn> PlayerPawn;
    FVector Location;
};

USTRUCT(BlueprintType)
struct FAttractorData
{
    GENERATED_BODY()

    UPROPERTY()
    TWeakObjectPtr<AActor> AttractorActor;

    float RadiusSq = 0.0f;
};

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

    // --- Spawn Points & Zone Management (Door System) ---
    void RegisterZombieSpawnPoints(const TArray<AZombieSpawnPoint*>& InSpawnPoints);

    UFUNCTION(BlueprintCallable, Category = "Zombie Director|Zones")
    void UnlockZone(int32 ZoneIDToUnlock);

    UFUNCTION(BlueprintCallable, Category = "Zombie Director|Zones")
    bool IsZoneUnlocked(int32 InZoneID) const { return UnlockedZones.Contains(InZoneID); }

    AZombieSpawnPoint* PickWeightedSpawnPoint(float MinSafeDist = 500.f, float MaxSpawnDist = 3000.f, bool bIsRelocation = false);

    // --- MysteryBox & FireSale API ---
    void RegisterMysteryBoxSpawnPoint(AMysteryBoxSpawnPoint* Point);
    void UnregisterMysteryBoxSpawnPoint(AMysteryBoxSpawnPoint* Point);
    AMysteryBoxSpawnPoint* GetRandomFreeMysteryBoxPoint(AMysteryBoxSpawnPoint* CurrentPoint);

    void RegisterMysteryBox(AMysteryBox* Box);
    void UnregisterMysteryBox(AMysteryBox* Box);

    void RegisterAttractor(AActor* AttractorActor, float Radius);
    void UnregisterAttractor(AActor* AttractorActor);

    void StartFireSale(float Duration);
    void EndFireSale();
    bool IsFireSaleActive() const { return bIsFireSaleActive; }

    UPROPERTY(EditDefaultsOnly, Category = "Mystery Box")
    TSubclassOf<AMysteryBox> MysteryBoxClass;

    const TArray<TWeakObjectPtr<AZombie>>& GetActiveZombies() const { return ActiveZombies; }
    TArray<FCachedTargetInfo, TInlineAllocator<4>> CachedPlayerLocations;

    void InitializeZombiePool(TSubclassOf<AZombie> InZombieClass, int32 PoolSize);
    AZombie* AcquireZombieFromPool(const FVector& SpawnLocation, const FRotator& SpawnRotation, int32 CurrentRound);
    void ReturnZombieToPool(AZombie* Zombie);

    bool bHasDirectLineOfSight = false;

    FORCEINLINE const TArray<TWeakObjectPtr<APawn>>& GetActivePlayers() const { return ActivePlayers; }

private:
    UPROPERTY()
    TArray<TObjectPtr<AZombie>> InactiveZombiePool;

    UPROPERTY()
    TSubclassOf<AZombie> PooledZombieClass;

    UPROPERTY()
    TArray<TWeakObjectPtr<APawn>> ActivePlayers;

    UPROPERTY()
    TArray<TWeakObjectPtr<APawn>> ValidTargetPlayers;

    UPROPERTY()
    TArray<TWeakObjectPtr<AZombie>> ActiveZombies;

    float PlayerCacheRefreshTimer = 0.f;

    UPROPERTY()
    TArray<TObjectPtr<AMysteryBoxSpawnPoint>> MysteryBoxSpawnPoints;

    UPROPERTY()
    TArray<TObjectPtr<AMysteryBoxSpawnPoint>> ReusableMysteryBoxPoints;

    UPROPERTY()
    TArray<TObjectPtr<AMysteryBox>> RegisteredBoxes;

    UPROPERTY()
    TArray<TObjectPtr<AMysteryBox>> TempFireSaleBoxes;

    UPROPERTY()
    TArray<TObjectPtr<AZombieSpawnPoint>> ZombieSpawnPoints;

    TSet<int32> UnlockedZones;
    int32 RoundRobinPlayerIndex = 0;

    FTimerHandle TimerHandle_FireSale;
    bool bIsFireSaleActive = false;

    FSpatialHashGrid2D SpatialGrid;

    int32 CurrentZombieIndex = 0;

    TWeakObjectPtr<AZombieSpawnPoint> LastUsedSpawnPoint;
    int32 ConsecutiveSpawnsOnLastPoint = 0;

    UPROPERTY(EditDefaultsOnly, Category = "Zombie Director|Spawning")
    int32 MaxConsecutiveSpawnsPerPoint = 2;

    UPROPERTY()
    TArray<FAttractorData> ActiveAttractors;

    void CleanInvalidAttractors();
    bool IsLocationVisibleToAnyPlayer(const FVector& WorldLoc, float MaxCheckDistSq) const;

public:
    void ExecuteElectricCherryBlast(AActor* InstigatorPawn, AController* InstigatorController, const FVector& Origin, float Radius, float FixedDamage);
 };