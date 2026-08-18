#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Curves/CurveFloat.h"
#include "RecoilComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LASTSTANDLEGACY_API URecoilComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    URecoilComponent();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    void AddRecoil(UCurveFloat* RecoilPitchCurve, int32 ShotsFired, float Multiplier = 1.0f, float Randomness = 0.5f, float RecoverySpeed = 15.0f);

    void ResetRecoil();

private:
    UPROPERTY()
    class APlayerController* OwnerController;

    FRotator CurrentRecoil;
    FRotator TargetRecoil;

    float LastFireTime;

    UPROPERTY(EditDefaultsOnly, Category = "Recoil Settings")
    float RecoilRecoveryDelay = 0.15f;

    UPROPERTY(EditDefaultsOnly, Category = "Recoil Settings")
    float RecoilReturnSpeed = 8.0f;

    float RecoilRecoverySpeed;
};