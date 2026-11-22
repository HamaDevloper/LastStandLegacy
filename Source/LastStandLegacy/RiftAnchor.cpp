#include "RiftAnchor.h"
#include "Components/SceneComponent.h"

#if WITH_EDITORONLY_DATA
#include "Components/ArrowComponent.h"
#endif

ARiftAnchor::ARiftAnchor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;
    SetCanBeDamaged(false);

    RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
    RootComponent = RootScene;

#if WITH_EDITORONLY_DATA
    FacingArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("FacingArrow"));
    FacingArrow->SetupAttachment(RootComponent);
    FacingArrow->ArrowColor = FColor::Cyan;
    FacingArrow->ArrowSize = 1.2f;
#endif
}