#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DoublePoint.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class URotatingMovementComponent; // فۆروارد دیکلارەیشن بۆ کۆمپۆنێنتە نوێیەکە

UCLASS()
class LASTSTANDLEGACY_API ADoublePoint : public AActor
{
    GENERATED_BODY()

public:
    ADoublePoint();

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* CollisionSphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* MeshComp;

    // کۆمپۆنێنتی ئۆپتیمایزکراو بۆ خولانەوە
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    URotatingMovementComponent* RotatingComp;

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UPROPERTY(EditAnywhere, Category = "PowerUp")
    float Duration = 30.0f;
};