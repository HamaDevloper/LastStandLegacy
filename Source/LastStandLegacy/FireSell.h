// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BasePowerUp.h"
#include "FireSell.generated.h"


UCLASS()
class LASTSTANDLEGACY_API AFireSell : public ABasePowerUp
{
	GENERATED_BODY()

protected:
    virtual void ActivatePowerUp(AHama* Player) override;

    UPROPERTY(EditDefaultsOnly, Category = "PowerUp")
    float FireSaleDuration = 30.0f;
	
};
