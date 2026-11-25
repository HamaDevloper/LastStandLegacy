// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GobbleGumAction.h"
#include "GobbleGumAction_SupplyLine.generated.h"

UCLASS()
class LASTSTANDLEGACY_API UGobbleGumAction_SupplyLine : public UGobbleGumAction
{
	GENERATED_BODY()
	
public:
    UGobbleGumAction_SupplyLine();

    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner) override;
    virtual void DeactivateAction_Implementation(ACharacter* CharacterOwner) override;
    float GetRemainingDuration() const;
private:
    FTimerHandle DurationTimerHandle;
    FTimerHandle AmmoRegenTimerHandle;
};