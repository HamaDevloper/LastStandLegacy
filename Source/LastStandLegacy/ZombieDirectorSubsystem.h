#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
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

    // UTickableWorldSubsystem overrides
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    // فەنکشنەکانی تۆمارکردنی زۆمبی
    void RegisterZombie(AZombie* Zombie);
    void UnregisterZombie(AZombie* Zombie);

    // فەنکشنەکانی تۆمارکردنی یاریزان
    void RegisterPlayer(APawn* Player);
    void UnregisterPlayer(APawn* Player);

    // فەنکشنێکی زۆر گرنگ بۆ Performance: تەنها ئەو یاریزانانە زیاد بکە کە دەکرێت ببنە ئامانج
    void SetPlayerTargetable(APawn* Player, bool bIsTargetable);

    void RegisterMysteryBoxSpawnPoint(AMysteryBoxSpawnPoint* Point);
    void UnregisterMysteryBoxSpawnPoint(AMysteryBoxSpawnPoint* Point);
    AMysteryBoxSpawnPoint* GetRandomFreeMysteryBoxPoint(AMysteryBoxSpawnPoint* CurrentPoint);

private:
    UPROPERTY()
    TArray<APawn*> ActivePlayers;

    UPROPERTY()
    TArray<APawn*> ValidTargetPlayers;

    UPROPERTY()
    TArray<TWeakObjectPtr<AZombie>> ActiveZombies;

    TArray<TWeakObjectPtr<APawn>> CachedPlayers;
    TArray<FVector> CachedPlayerLocations;
    float PlayerCacheRefreshTimer = 0.f;

    UPROPERTY()
    TArray<TObjectPtr<AMysteryBoxSpawnPoint>> MysteryBoxSpawnPoints;

    // --- ڕێکخستنەکانی Time-Slicing بۆ Optimization ---

    // ئیندێکسی ئەو زۆمبییەی کە ئێستا نۆرەیەتی ئەپدەیت بکرێت
    int32 CurrentZombieIndex = 0;

    // ژمارەی ئەو زۆمبییانەی لە هەر فڕەیمێکدا حیساباتیان بۆ دەکرێت (٥ زۆر گونجاوە)
    int32 ZombiesToUpdatePerFrame = 5;

    // ئەگەر یاریزان لە ١٥٠ یەکە کەمتر جوڵابوو، NavMesh دووبارە حیسابات ناکاتەوە (دووجا کراوە بۆ خێرایی)
    float PathUpdateDistanceThresholdSq = FMath::Square(150.f);
};