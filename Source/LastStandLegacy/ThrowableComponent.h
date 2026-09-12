#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "ThrowableComponent.generated.h"

class UAnimMontage;
class AHama;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnThrowableCountChanged, int32, NewCount);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LASTSTANDLEGACY_API UThrowableComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UThrowableComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    void StartThrowCharge();

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    void ReleaseThrow();

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    void RefillThrowables(int32 Amount = 3);

    bool IsThrowingInProcess() const { return bIsThrowingInProcess; }

protected:
    virtual void BeginPlay() override;

    UFUNCTION(Server, Reliable)
    void Server_StartThrowCharge();

    UFUNCTION(Server, Reliable)
    void Server_ReleaseThrow(FVector_NetQuantizeNormal LaunchDirection);

    UFUNCTION(Client, Reliable)
    void Client_ResetThrowState();

    UFUNCTION()
    void OnRep_ThrowableCount();

    UFUNCTION()
    void OnRep_IsCharging();

    UFUNCTION()
    void ResetThrowState_Server();

    UFUNCTION()
    void OnThrowMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    void SetWeaponVisibility(bool bVisible);

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Config")
    TSubclassOf<AActor> ThrowableClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Config")
    TObjectPtr<UAnimMontage> ThrowMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Throwable|Anim")
    FName HoldSectionName = TEXT("Hold");

    UPROPERTY(EditDefaultsOnly, Category = "Throwable|Anim")
    FName ReleaseSectionName = TEXT("Release");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Config")
    float ThrowImpulseStrength = 1200.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Config")
    FName HandSocketName = TEXT("Muzzle_R_Hand");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Config")
    int32 MaxThrowableCount = 3;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Config")
    float ThrowCooldown = 1.0f;

    UPROPERTY(ReplicatedUsing = OnRep_ThrowableCount, EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|State")
    int32 CurrentThrowableCount = 3;

    UPROPERTY(Replicated)
    bool bIsThrowingInProcess = false;

    UPROPERTY(BlueprintAssignable, Category = "Throwable|Events")
    FOnThrowableCountChanged OnThrowableCountChanged;

private:
    UPROPERTY()
    TObjectPtr<AHama> CharacterOwner;

    UPROPERTY()
    FVector CachedThrowStartLoc;

    float LastThrowTime = -100.0f;

    UPROPERTY(ReplicatedUsing = OnRep_IsCharging)
    uint8 bIsCharging : 1;

    FTimerHandle TimerHandle_ResetThrowState;

    void GetSafeSpawnLocation(const FVector& StartLoc, const FVector& TargetLoc, FVector& OutSpawnLoc) const;
    FVector GetCameraAimDirection() const;
    FVector GetCrosshairTargetPoint(const FVector& AimDir) const;
};