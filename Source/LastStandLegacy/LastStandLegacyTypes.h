#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "LastStandLegacyTypes.generated.h"

UENUM(BlueprintType)
enum class EPowerUpType : uint8
{
    None,
    DoublePoints,
    MaxAmmo,
    InstaKill,
    Nuke,
    DeathMachine,
    MaxPower,
    BonusPoints,
    FireSale
};

USTRUCT(BlueprintType)
struct FPowerUpData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UTexture2D* Icon;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsTimed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Duration;

    FPowerUpData()
        : DisplayName(FText::GetEmpty())
        , Icon(nullptr)
        , bIsTimed(false)
        , Duration(0.0f)
    {
    }
};