#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Zombie.h"
#include "ZombieAnimInstance.generated.h"

UCLASS()
class LASTSTANDLEGACY_API UZombieAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    UPROPERTY(BlueprintReadOnly, meta = (BlueprintThreadSafe))
    float GroundSpeed = 0.f;

private:
    UPROPERTY()
    TObjectPtr<AZombie> Zombie;
};