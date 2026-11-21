// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GobbleGumAction.h"
#include "GobbleGumAction_Overclocked.generated.h"

class UGobbleGumComponent;

/**
 * 
 */
UCLASS(Blueprintable, meta = (DisplayName = "GobbleGum Action: Overclocked Circuit"))
class LASTSTANDLEGACY_API UGobbleGumAction_Overclocked : public UGobbleGumAction
{
	GENERATED_BODY()

public:
    UGobbleGumAction_Overclocked();

    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner) override;
    virtual void DeactivateAction_Implementation(ACharacter* CharacterOwner) override;


private:
    TWeakObjectPtr<UGobbleGumComponent> CachedGumComponent;
};
