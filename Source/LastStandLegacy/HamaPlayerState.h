// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "HamaPlayerState.generated.h"

/**
 * 
 */
UCLASS()
class LASTSTANDLEGACY_API AHamaPlayerState : public APlayerState
{
	GENERATED_BODY()

    AHamaPlayerState();

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(ReplicatedUsing = OnRep_Points, BlueprintReadOnly, Category = "Player State")
    int32 Points;

    UFUNCTION()
    void OnRep_Points();

public:
    UFUNCTION(BlueprintCallable, Category = "Player State")
    void AddPoints(int32 Amount);
	
};
