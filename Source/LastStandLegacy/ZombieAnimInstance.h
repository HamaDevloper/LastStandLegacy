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

    UPROPERTY(BlueprintReadOnly, Category = "Animation")
    float GroundSpeed;

private:
    UPROPERTY()
    TObjectPtr<AZombie> Zombie;
};