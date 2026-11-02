#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ThrowablePoolSubsystem.generated.h"

class AGrenade;
class AMonkeyBomb;

UCLASS()
class LASTSTANDLEGACY_API UThrowablePoolSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** دابینکردنی نارنجۆک لە پووڵ بە وەرگرتنی TSubclassOf<AActor> */
    AGrenade* AcquireGrenade(TSubclassOf<AActor> GrenadeClass, const FTransform& SpawnTransform);

    /** دابینکردنی مۆنکی لە پووڵ بە وەرگرتنی TSubclassOf<AActor> */
    AMonkeyBomb* AcquireMonkey(TSubclassOf<AActor> MonkeyClass, const FTransform& SpawnTransform);

    /** گەڕاندنەوەی نارنجۆک بۆ ناو پووڵ */
    void ReturnGrenade(AGrenade* Grenade);

    /** گەڕاندنەوەی مۆنکی بۆ ناو پووڵ */
    void ReturnMonkey(AMonkeyBomb* Monkey);

private:
    UPROPERTY()
    TArray<TObjectPtr<AGrenade>> InactiveGrenades;

    UPROPERTY()
    TArray<TObjectPtr<AMonkeyBomb>> InactiveMonkeys;

    static constexpr int32 InitialReserveCount = 10;

public:
    void WarmupPool(TSubclassOf<AActor> GrenadeClass, int32 GrenadeCount, TSubclassOf<AActor> MonkeyClass, int32 MonkeyCount);
};