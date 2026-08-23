#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/StreamableManager.h"
#include "WeaponManagerSubsystem.generated.h"

UCLASS()
class LASTSTANDLEGACY_API UWeaponManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:

    void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category = "Weapon System")
    void PreloadWeaponsFromDataTable(UDataTable* WeaponDataTable);

    UFUNCTION(BlueprintCallable, Category = "Weapon System")
    void ClearWeaponCache();

private:
    UPROPERTY()
    TSet<TObjectPtr<UObject>> LoadedAssetsCache;

    FStreamableManager StreamableManager;

    TArray<TSharedPtr<FStreamableHandle>> ActivePreloadHandles;
};