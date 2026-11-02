#include "ThrowablePoolSubsystem.h"
#include "Grenade.h"
#include "MonkeyBomb.h"
#include "Engine/World.h"

void UThrowablePoolSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    InactiveGrenades.Reserve(InitialReserveCount);
    InactiveMonkeys.Reserve(InitialReserveCount);
}

void UThrowablePoolSubsystem::Deinitialize()
{
    InactiveGrenades.Empty();
    InactiveMonkeys.Empty();

    Super::Deinitialize();
}

AGrenade* UThrowablePoolSubsystem::AcquireGrenade(TSubclassOf<AActor> GrenadeClass, const FTransform& SpawnTransform)
{
    UWorld* World = GetWorld();
    if (!World || !World->GetAuthGameMode())
    {
        return nullptr;
    }

    AGrenade* RetrievedGrenade = nullptr;

    while (InactiveGrenades.Num() > 0)
    {
        RetrievedGrenade = InactiveGrenades.Pop(EAllowShrinking::No);
        if (IsValid(RetrievedGrenade))
        {
            break;
        }
        RetrievedGrenade = nullptr;
    }

    if (!RetrievedGrenade)
    {
        if (!GrenadeClass)
        {
            return nullptr;
        }

        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        RetrievedGrenade = World->SpawnActor<AGrenade>(GrenadeClass, SpawnTransform, SpawnParams);
    }
    else
    {
        RetrievedGrenade->SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
    }

    return RetrievedGrenade;
}

void UThrowablePoolSubsystem::ReturnGrenade(AGrenade* Grenade)
{
    if (!IsValid(Grenade))
    {
        return;
    }

    if (!InactiveGrenades.Contains(Grenade))
    {
        InactiveGrenades.Add(Grenade);
    }
}

AMonkeyBomb* UThrowablePoolSubsystem::AcquireMonkey(TSubclassOf<AActor> MonkeyClass, const FTransform& SpawnTransform)
{
    UWorld* World = GetWorld();
    if (!World || !World->GetAuthGameMode())
    {
        return nullptr;
    }

    AMonkeyBomb* RetrievedMonkey = nullptr;

    while (InactiveMonkeys.Num() > 0)
    {
        RetrievedMonkey = InactiveMonkeys.Pop(EAllowShrinking::No);
        if (IsValid(RetrievedMonkey))
        {
            break;
        }
        RetrievedMonkey = nullptr;
    }

    if (!RetrievedMonkey)
    {
        if (!MonkeyClass)
        {
            return nullptr;
        }

        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        RetrievedMonkey = World->SpawnActor<AMonkeyBomb>(MonkeyClass, SpawnTransform, SpawnParams);
    }
    else
    {
        RetrievedMonkey->SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
    }

    return RetrievedMonkey;
}

void UThrowablePoolSubsystem::ReturnMonkey(AMonkeyBomb* Monkey)
{
    if (!IsValid(Monkey))
    {
        return;
    }

    if (!InactiveMonkeys.Contains(Monkey))
    {
        InactiveMonkeys.Add(Monkey);
    }
}

void UThrowablePoolSubsystem::WarmupPool(TSubclassOf<AActor> GrenadeClass, int32 GrenadeCount, TSubclassOf<AActor> MonkeyClass, int32 MonkeyCount)
{
    UWorld* World = GetWorld();
    if (!World || !World->GetAuthGameMode()) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // سپۆنکردنی پێشوەختەی نارنجۆکەکان لە دوورترین خاڵ و ڕاستەوخۆ خەواندنیان لە ناو پووڵدا
    if (GrenadeClass)
    {
        for (int32 i = 0; i < GrenadeCount; ++i)
        {
            FTransform SpawnTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, -500.0f));
            if (AGrenade* NewGrenade = World->SpawnActor<AGrenade>(GrenadeClass, SpawnTransform, SpawnParams))
            {
                NewGrenade->PrepareForPool();
            }
        }
    }

    // سپۆنکردنی پێشوەختەی مەیموونەکان
    if (MonkeyClass)
    {
        for (int32 i = 0; i < MonkeyCount; ++i)
        {
            FTransform SpawnTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, -50000.0f));
            if (AMonkeyBomb* NewMonkey = World->SpawnActor<AMonkeyBomb>(MonkeyClass, SpawnTransform, SpawnParams))
            {
                NewMonkey->PrepareForPool();
            }
        }
    }
}