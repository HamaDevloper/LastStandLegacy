#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "ThrowableComponent.generated.h"

class AHama;
class UAnimMontage;

DECLARE_DELEGATE_TwoParams(FOnThrowableCountChanged, int32 /*MonkeyCount*/, int32 /*GrenadeCount*/);

UENUM(BlueprintType)
enum class EThrowChargeState : uint8
{
    Idle,
    Charging,
    Released
};

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
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

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

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    bool IsThrowingInProcess() const { return bIsThrowingLocal || ChargeState != EThrowChargeState::Idle; }

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    bool IsCharging() const { return ChargeState == EThrowChargeState::Charging; }

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    TSubclassOf<AActor> GetMonkeyClass() const { return MonkeyClass; }

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    int32 GetCurrentGrenadeCount() const { return CurrentGrenadeCount; }

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    int32 GetCurrentMonkeyCount() const { return CurrentMonkeyCount; }

    void ToggleHandThrowableVisibility(bool bVisible);
    void Server_OnGrenadeCookExpired();

    int32 GetMaxGrenadeCount() const { return MaxGrenadeCount; }
    int32 GetMaxMonkeyCount() const { return MaxMonkeyCount; }

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Throwable|Config")
    TSubclassOf<AActor> GrenadeClass;

    UPROPERTY(EditDefaultsOnly, Category = "Throwable|Config")
    TSubclassOf<AActor> MonkeyClass;

    UPROPERTY(EditDefaultsOnly, Category = "Throwable|Config")
    TObjectPtr<UAnimMontage> GrenadeThrowMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Throwable|Config")
    TObjectPtr<UAnimMontage> MonkeyThrowMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Throwable|Config")
    FName HoldSectionName = FName("Hold");

    UPROPERTY(EditDefaultsOnly, Category = "Throwable|Config")
    FName ReleaseSectionName = FName("Release");

    UPROPERTY(EditDefaultsOnly, Category = "Throwable|Config")
    FName ThrowableHandSocketName = TEXT("GrenadeHandSocket");

    UPROPERTY(EditDefaultsOnly, Category = "Throwable|Config")
    float GrenadeThrowImpulseStrength = 1500.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Throwable|Config")
    float MonkeyThrowImpulseStrength = 1500.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Throwable|Config")
    int32 MaxGrenadeCount = 5;

    UPROPERTY(EditDefaultsOnly, Category = "Throwable|Config")
    int32 MaxMonkeyCount = 3;

    UPROPERTY(EditDefaultsOnly, Category = "Throwable|Config")
    float MaxGrenadeCookTime = 3.5f;

    UPROPERTY(Transient)
    float GrenadeThrowCooldown = 1.0f;

    UPROPERTY(Transient)
    float MonkeyThrowCooldown = 1.0f;

    UPROPERTY(ReplicatedUsing = OnRep_GrenadeCount, VisibleInstanceOnly, Category = "Throwable|State")
    uint8 CurrentGrenadeCount;

    UPROPERTY(ReplicatedUsing = OnRep_MonkeyCount, VisibleInstanceOnly, Category = "Throwable|State")
    uint8 CurrentMonkeyCount;

    UPROPERTY(ReplicatedUsing = OnRep_ChargeState, VisibleInstanceOnly, Category = "Throwable|State")
    EThrowChargeState ChargeState = EThrowChargeState::Idle;

    UPROPERTY(Replicated)
    bool bIsMonkeyThrow = false;

    UPROPERTY(Transient)
    bool bIsThrowingLocal = false;

    UPROPERTY(Transient)
    float LastThrowTime = -100.0f;

    float ServerCookStartTime = 0.0f;

    FTimerHandle TimerHandle_ResetThrowState;

    UPROPERTY()
    TObjectPtr<AHama> CharacterOwner;

    UPROPERTY()
    TObjectPtr<AActor> HeldThrowableVisualActor;

    UFUNCTION(Server, Reliable)
    void Server_StartCharge(bool bIsMonkey);

    UFUNCTION(Server, Reliable)
    void Server_ExecuteThrow(FVector_NetQuantizeNormal LaunchDirection, bool bIsMonkey);

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_OnGrenadeCookExpiredFX();

    void ExecuteStartCharge_Server(bool bIsMonkey);
    
    UFUNCTION()
    void OnRep_GrenadeCount();

    UFUNCTION()
    void OnRep_MonkeyCount();

    UFUNCTION()
    void OnRep_ChargeState();

    UFUNCTION(Client, Reliable)
    void Client_RejectThrow();

    void Local_OnGrenadeCookExpired();

    UFUNCTION()
    void OnThrowMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    void Internal_StartCharge(TSubclassOf<AActor> ThrowableClass, int32 CurrentCount, UAnimMontage* MontageToPlay, bool bIsMonkey);
    void Internal_ReleaseThrow(TSubclassOf<AActor> ThrowableClass, int32 CurrentCount, UAnimMontage* MontageToPlay, bool bIsMonkey);

    void ResetThrowState_Server();
    void SetWeaponHidden(bool bHidden);
    void HandleChargeStateChanged();

    FVector GetCrosshairTargetPoint(FVector& OutAimDir) const;
    void GetSafeSpawnLocation(const FVector& StartLoc, const FVector& TargetLoc, FVector& OutSpawnLoc) const;

    FTimerHandle TimerHandle_CookExplosion;

private:
    void Debug_RenderNetworkDesync();
    void PlayThrowMontageWithDelegate(UAnimMontage* MontageToPlay, FName SectionName = NAME_None);
};