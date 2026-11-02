#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "ThrowableComponent.generated.h"

class AHama;
class UAnimMontage;
class AGrenade;
class AMonkeyBomb;

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
    FOnThrowableCountChanged OnThrowableCountChanged;

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    void StartGrenadeCharge();

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    void ReleaseGrenadeThrow();

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    void StartMonkeyCharge();

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    void ReleaseMonkeyThrow();

    // بە TSubclassOf<AActor> دەمێنێتەوە بۆ ئەوەی لەگەڵ Mystery Box و بانگکەرەکانی تر گونجاو بێت
    UFUNCTION(BlueprintCallable, Category = "Throwable")
    void UnlockAndRefillMonkeyBomb(TSubclassOf<AActor> NewMonkeyClass, int32 Amount = 3);

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    void RefillMonkeyToMax();

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    void RefillGrenadesToMax();

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    FORCEINLINE bool IsThrowingInProcess() const { return bIsThrowingLocal || ChargeState != EThrowChargeState::Idle; }

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    FORCEINLINE bool IsCharging() const { return ChargeState == EThrowChargeState::Charging; }

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    TSubclassOf<AActor> GetMonkeyClass() const { return MonkeyClass; }

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    FORCEINLINE int32 GetCurrentGrenadeCount() const { return CurrentGrenadeCount; }

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    FORCEINLINE int32 GetCurrentMonkeyCount() const { return CurrentMonkeyCount; }

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    FORCEINLINE int32 GetMaxGrenadeCount() const { return MaxGrenadeCount; }

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    FORCEINLINE int32 GetMaxMonkeyCount() const { return MaxMonkeyCount; }

    void ToggleHandThrowableVisibility(bool bVisible);
    void HandleOwnerDowned();

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
    uint8 CurrentGrenadeCount = 3;

    UPROPERTY(ReplicatedUsing = OnRep_MonkeyCount, VisibleInstanceOnly, Category = "Throwable|State")
    uint8 CurrentMonkeyCount = 0;

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
    FTimerHandle TimerHandle_CookExplosion;
    FTimerHandle TimerHandle_LocalCookExplosion;

    UPROPERTY()
    TObjectPtr<AHama> CharacterOwner;

    // پارامیتەرەکان بە دەقیقی لەگەڵ .cpp هاوتا کراون
    UFUNCTION(Server, Reliable)
    void Server_StartCharge(bool bIsMonkey);

    UFUNCTION(Server, Reliable)
    void Server_ExecuteThrow(FVector_NetQuantize ClientViewLoc, FVector_NetQuantizeNormal ClientAimDir, bool bIsMonkey);

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_OnGrenadeCookExpiredFX();

    UFUNCTION(Client, Reliable)
    void Client_RejectThrow();

    void ExecuteStartCharge_Server(bool bIsMonkey);
    void Server_OnGrenadeCookExpired();
    void Local_OnGrenadeCookExpired();

    UFUNCTION()
    void OnRep_GrenadeCount();

    UFUNCTION()
    void OnRep_MonkeyCount();

    UFUNCTION()
    void OnRep_ChargeState();

    UFUNCTION()
    void OnThrowMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    void Internal_StartCharge(bool bIsMonkey);
    void Internal_ReleaseThrow(bool bIsMonkey);

    void ResetThrowState_Server();
    void SetWeaponHidden(bool bHidden);
    void HandleChargeStateChanged();

    FVector TraceCrosshairTarget(const FVector& ViewLoc, const FVector& AimDir) const;
    void GetSafeSpawnLocation(const FVector& StartLoc, const FVector& TargetLoc, FVector& OutSpawnLoc) const;

private:
    void PlayThrowMontageWithDelegate(UAnimMontage* MontageToPlay, FName SectionName = NAME_None);

    uint8 PendingGrenadeThrows = 0;
    uint8 PendingMonkeyThrows = 0;
};