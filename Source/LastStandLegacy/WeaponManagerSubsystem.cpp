#include "WeaponManagerSubsystem.h"
#include "BaseWeapon.h"
#include "Engine/AssetManager.h"

void UWeaponManagerSubsystem::Deinitialize()
{
    ClearWeaponCache();
    Super::Deinitialize();
}

void UWeaponManagerSubsystem::PreloadWeaponsFromDataTable(UDataTable* WeaponDataTable)
{
    if (!WeaponDataTable || IsRunningDedicatedServer())
    {
        return;
    }

    TArray<FSoftObjectPath> AssetsToLoad;
    static const FString ContextString(TEXT("WeaponPreloadContext"));

    TArray<FWeaponData*> AllRows;
    WeaponDataTable->GetAllRows<FWeaponData>(ContextString, AllRows);

    for (const FWeaponData* Row : AllRows)
    {
        if (!Row) continue;

        if (!Row->WeaponMeshAsset.IsNull())  AssetsToLoad.AddUnique(Row->WeaponMeshAsset.ToSoftObjectPath());
        if (!Row->WeaponIdle.IsNull())       AssetsToLoad.AddUnique(Row->WeaponIdle.ToSoftObjectPath());
        if (!Row->WeaponSprint.IsNull())     AssetsToLoad.AddUnique(Row->WeaponSprint.ToSoftObjectPath());
        if (!Row->AimSequence.IsNull())      AssetsToLoad.AddUnique(Row->AimSequence.ToSoftObjectPath());
        if (!Row->ReloadMontage.IsNull())    AssetsToLoad.AddUnique(Row->ReloadMontage.ToSoftObjectPath());
        if (!Row->AimOffsetAsset.IsNull())   AssetsToLoad.AddUnique(Row->AimOffsetAsset.ToSoftObjectPath());
    }

    if (AssetsToLoad.Num() == 0) return;

    TWeakObjectPtr<UWeaponManagerSubsystem> WeakThis(this);

    TSharedPtr<FStreamableHandle> NewHandle = StreamableManager.RequestAsyncLoad(AssetsToLoad, FStreamableDelegate::CreateLambda([WeakThis, AssetsToLoad]()
        {
            if (!WeakThis.IsValid()) return;

            for (const FSoftObjectPath& Path : AssetsToLoad)
            {
                if (UObject* LoadedAsset = Path.ResolveObject())
                {
                    WeakThis->LoadedAssetsCache.Add(LoadedAsset);
                }
            }
        }));

    if (NewHandle.IsValid())
    {
        ActivePreloadHandles.Add(NewHandle);
    }
}

void UWeaponManagerSubsystem::ClearWeaponCache()
{
    for (TSharedPtr<FStreamableHandle>& Handle : ActivePreloadHandles)
    {
        if (Handle.IsValid())
        {
            if (Handle->IsActive())
            {
                Handle->CancelHandle();
            }
            Handle->ReleaseHandle();
        }
    }

    ActivePreloadHandles.Empty();
    LoadedAssetsCache.Empty();
}