// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractInterface.generated.h"

class AHama;

UINTERFACE(MinimalAPI)
class UInteractInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class LASTSTANDLEGACY_API IInteractInterface
{
	GENERATED_BODY()

public:
   virtual void Interact(AHama* Hama) = 0;
   virtual FString GetInteractMessage() = 0;
};
