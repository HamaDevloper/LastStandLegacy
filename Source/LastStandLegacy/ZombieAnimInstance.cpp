#include "ZombieAnimInstance.h"
#include "GameFramework/Character.h"

void UZombieAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    Zombie = Cast<AZombie>(TryGetPawnOwner());
}

void UZombieAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    GroundSpeed = Zombie->GetVelocity().Size2D();
}