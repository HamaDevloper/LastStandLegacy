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

    if (!Zombie)
    {
        Zombie = Cast<AZombie>(TryGetPawnOwner());
        if (!Zombie) return;
    }

    GroundSpeed = Zombie->GetVelocity().Size2D();
}