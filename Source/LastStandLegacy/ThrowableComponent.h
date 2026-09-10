#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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

    // BO3-Style Hold & Release API
    UFUNCTION(BlueprintCallable, Category = "Throwable")
    void StartThrowCharge();

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    void ReleaseThrow();

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    void RefillThrowables(int32 Amount = 3);

protected:
    virtual void BeginPlay() override;

    UFUNCTION(Server, Reliable)
    void Server_StartThrowCharge();

    UFUNCTION(Server, Reliable)
    void Server_ReleaseThrow(FVector_NetQuantizeNormal LaunchDirection);

    UFUNCTION()
    void OnRep_ThrowableCount();

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

    // State Properties
    UPROPERTY(ReplicatedUsing = OnRep_ThrowableCount, EditDefaultsOnly,  BlueprintReadOnly, Category = "Throwable|State")
    int32 CurrentThrowableCount = 3;

    UPROPERTY(Replicated)
    bool bIsThrowingInProcess = false;

    UPROPERTY(BlueprintAssignable, Category = "Throwable|Events")
    FOnThrowableCountChanged OnThrowableCountChanged;

    bool IsThrowingInProcess() const { return bIsThrowingInProcess; }

private:
    UPROPERTY()
    TObjectPtr<AHama> CharacterOwner;

    float LastThrowTime = 0.0f;

    UPROPERTY(ReplicatedUsing = OnRep_IsCharging)
    uint8 bIsCharging : 1;

    UFUNCTION()
    void OnRep_IsCharging();

    void GetSafeSpawnLocation(const FVector& StartLoc, const FVector& TargetLoc, FVector& OutSpawnLoc) const;
    FVector GetCrosshairAimDirection() const;
};