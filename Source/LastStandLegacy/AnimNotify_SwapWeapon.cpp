#include "AnimNotify_SwapWeapon.h"
#include "Hama.h"

void UAnimNotify_SwapWeapon::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    if (!MeshComp) return;

    if (AHama* Character = Cast<AHama>(MeshComp->GetOwner()))
    {
        Character->HandleWeaponSwapNotify();
    }
}