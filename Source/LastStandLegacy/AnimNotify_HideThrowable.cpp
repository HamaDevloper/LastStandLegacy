
#include "AnimNotify_HideThrowable.h"
#include "ThrowableComponent.h"

void UAnimNotify_HideThrowable::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    if (!MeshComp) return;

    AActor* OwnerActor = MeshComp->GetOwner();
    if (!OwnerActor) return;

    if (UThrowableComponent* ThrowableComp = OwnerActor->FindComponentByClass<UThrowableComponent>())
    {
        ThrowableComp->ToggleHandThrowableVisibility(false);
    }
}