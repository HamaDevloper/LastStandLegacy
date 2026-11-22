#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RiftAnchor.generated.h"

class UArrowComponent;

UCLASS()
class LASTSTANDLEGACY_API ARiftAnchor : public AActor
{
    GENERATED_BODY()

public:
    ARiftAnchor();

    // ژمارەی ئەو ناوچەیەی ئەم خاڵەی تێدایە (هاوتای UnlockZoneIDـی ABaseDoor). ژمارە 0 واتە ژووری سەرەتا
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "RiftAnchor")
    int32 ZoneID = 0;

    // ئەگەر ئەم خاڵە لە ناوچەی سەرەتای یارییەکەدایە و بێ کردنەوەی دەرگا کراوەیە، ئەمە True بکە
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "RiftAnchor")
    bool bStartsUnlocked = false;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> RootScene;

#if WITH_EDITORONLY_DATA
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UArrowComponent> FacingArrow;
#endif
};