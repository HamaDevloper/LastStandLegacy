#include "AnimNotify_MeleeHit.h"
#include "Hama.h"

void UAnimNotify_MeleeHit::Notify(
    USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference
)
{
    if (!MeshComp)
        return;

    if (AHama* Hama = Cast<AHama>(MeshComp->GetOwner()))
    {
        Hama->PerformMeleeHitDetection();
    }
}