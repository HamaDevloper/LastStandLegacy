// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DamageableInterface.generated.h"

UINTERFACE(MinimalAPI)
class UDamageableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class LASTSTANDLEGACY_API IDamageableInterface
{
	GENERATED_BODY()

public:
    virtual bool CanReceiveWeaponDamage() const = 0;
};
