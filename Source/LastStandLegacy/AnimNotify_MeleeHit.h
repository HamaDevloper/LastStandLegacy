// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_MeleeHit.generated.h"

/**
 * 
 */
UCLASS()
class LASTSTANDLEGACY_API UAnimNotify_MeleeHit : public UAnimNotify
{
	GENERATED_BODY()

public:
    virtual void Notify(
        USkeletalMeshComponent* MeshComp,
        UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference
    ) override;
};
