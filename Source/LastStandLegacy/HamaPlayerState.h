#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "HamaPlayerState.generated.h"

// ١. دروستکردنی دیسپاچەر (Dynamic Multicast Delegate) بۆ پۆینت و کیڵ
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPointsChangedSignature, int32, NewPoints);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnKillsChangedSignature, int32, NewKills);

UCLASS()
class LASTSTANDLEGACY_API AHamaPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    AHamaPlayerState();

    // ٢. ناساندنی دیسپاچەرەکان وەک متغییرێک کە لە بلوپرینتدا وەک Event دەرکەون
    UPROPERTY(BlueprintAssignable, Category = "Player State | Events")
    FOnPointsChangedSignature OnPointsChanged;

    UPROPERTY(BlueprintAssignable, Category = "Player State | Events")
    FOnKillsChangedSignature OnKillsChanged;

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(ReplicatedUsing = OnRep_Points, BlueprintReadOnly, Category = "Player State")
    int32 Points;

    UPROPERTY(ReplicatedUsing = OnRep_Kills, BlueprintReadOnly, Category = "Player State")
    int32 Kills;

    UFUNCTION()
    void OnRep_Points();

    UFUNCTION()
    void OnRep_Kills();

public:
    UFUNCTION(BlueprintCallable, Category = "Player State")
    void AddPoints(int32 Amount);

    UFUNCTION(BlueprintCallable, Category = "Player State")
    void AddKills(int32 Amount);

    UFUNCTION(BlueprintCallable, Category = "Player State")
    int32 GetPoints() const { return Points; }

    UFUNCTION(BlueprintCallable, Category = "Player State")
    int32 GetKills() const { return Kills; }
};