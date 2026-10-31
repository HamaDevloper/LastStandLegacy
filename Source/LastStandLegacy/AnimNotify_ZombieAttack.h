// AnimNotify_ZombieAttack.h
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ZombieAttack.generated.h"

UCLASS()
class LASTSTANDLEGACY_API UAnimNotify_ZombieAttack : public UAnimNotify
{
    GENERATED_BODY()

public:
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};