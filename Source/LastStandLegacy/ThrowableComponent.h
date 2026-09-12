#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "ThrowableComponent.generated.h"

class AHama;
class UAnimMontage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnThrowableCountChanged, int32, MonkeyCount, int32, GrenadeCount);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LASTSTANDLEGACY_API UThrowableComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UThrowableComponent();

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
    // =========================================================================
    // 🎯 DELEGATES & PUBLIC API
    // =========================================================================

    UPROPERTY(BlueprintAssignable, Category = "Throwable|Events")
    FOnThrowableCountChanged OnThrowableCountChanged;

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    void StartGrenadeCharge();

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    void ReleaseGrenadeThrow();

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    void StartMonkeyCharge();

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    void ReleaseMonkeyThrow();

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    void UnlockAndRefillMonkeyBomb(TSubclassOf<AActor> NewMonkeyClass, int32 Amount = 3);

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    void RefillMonkeyToMax();

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    void RefillGrenadesToMax();

    // 🛠️ GETTERS FOR CHARACTER / WEAPON / INTERACTION CHECKS
    UFUNCTION(BlueprintCallable, Category = "Throwable")
    bool IsThrowingInProcess() const { return bIsCharging || bIsThrowingInProcess; }

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    bool IsCharging() const { return bIsCharging; }

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    TSubclassOf<AActor> GetMonkeyClass() const { return MonkeyClass; }

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    int32 GetCurrentGrenadeCount() const { return CurrentGrenadeCount; }

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    int32 GetCurrentMonkeyCount() const { return CurrentMonkeyCount; }

protected:
    // =========================================================================
    // ⚙️ CONFIGURATION & PROPERTIES
    // =========================================================================

    UPROPERTY(EditDefaultsOnly, Category = "Throwable|Config")
    TSubclassOf<AActor> GrenadeClass;

    UPROPERTY(EditDefaultsOnly, Category = "Throwable|Config")
    TSubclassOf<AActor> MonkeyClass;

    UPROPERTY(EditDefaultsOnly, Category = "Throwable|Config")
    UAnimMontage* GrenadeThrowMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Throwable|Config")
    UAnimMontage* MonkeyThrowMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Throwable|Config")
    FName HoldSectionName = FName("Hold");

    UPROPERTY(EditDefaultsOnly, Category = "Throwable|Config")
    FName ReleaseSectionName = FName("Release");

    UPROPERTY(EditDefaultsOnly, Category = "Throwable|Config")
    float ThrowImpulseStrength = 1500.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Throwable|Config")
    int32 MaxGrenadeCount = 5;

    UPROPERTY(EditDefaultsOnly, Category = "Throwable|Config")
    int32 MaxMonkeyCount = 3;

    // =========================================================================
    // 🌐 REPLICATED STATE
    // =========================================================================

    UPROPERTY(ReplicatedUsing = OnRep_GrenadeCount)
    int32 CurrentGrenadeCount;

    UPROPERTY(ReplicatedUsing = OnRep_MonkeyCount)
    int32 CurrentMonkeyCount;

    UPROPERTY(ReplicatedUsing = OnRep_IsCharging)
    bool bIsCharging;

    bool bIsThrowingInProcess;
    float LastThrowTime;
    float ThrowCooldown;

    FTimerHandle TimerHandle_ResetThrowState;

    UPROPERTY()
    AHama* CharacterOwner;

    // =========================================================================
    // 📡 NETWORKING & INTERNAL HELPER FUNCTIONS
    // =========================================================================

    UFUNCTION(Server, Reliable)
    void Server_StartCharge();

    UFUNCTION(Server, Reliable)
    void Server_ExecuteThrow(FVector_NetQuantizeNormal LaunchDirection, bool bIsMonkey);

    UFUNCTION(Client, Reliable)
    void Client_ResetThrowState();

    UFUNCTION()
    void OnRep_GrenadeCount();

    UFUNCTION()
    void OnRep_MonkeyCount();

    UFUNCTION()
    void OnRep_IsCharging();

    UFUNCTION()
    void OnThrowMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    void Internal_StartCharge(TSubclassOf<AActor> ThrowableClass, int32 CurrentCount, UAnimMontage* MontageToPlay);
    void Internal_ReleaseThrow(TSubclassOf<AActor> ThrowableClass, int32 CurrentCount, UAnimMontage* MontageToPlay, bool bIsMonkey);

    void ResetThrowState_Server();
    void SetWeaponHidden(bool bHidden);

    FVector GetCameraAimDirection() const;
    FVector GetCrosshairTargetPoint(const FVector& AimDir) const;
    void GetSafeSpawnLocation(const FVector& StartLoc, const FVector& TargetLoc, FVector& OutSpawnLoc) const;
};