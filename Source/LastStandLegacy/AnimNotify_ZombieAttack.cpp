#include "AnimNotify_ZombieAttack.h"
#include "Zombie.h"

void UAnimNotify_ZombieAttack::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    if (!MeshComp) return;

    if (AZombie* Zombie = Cast<AZombie>(MeshComp->GetOwner()))
    {
        if (Zombie->HasAuthority())
        {
            Zombie->ExecuteMeleeHit();
        }
    }
}