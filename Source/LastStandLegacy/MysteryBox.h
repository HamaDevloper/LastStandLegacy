#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractInterface.h"
#include "Components/TimelineComponent.h"
#include "MysteryBox.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class AHama;
class ABaseWeapon;
class AMysteryBoxSpawnPoint;
class USoundBase;
class UNiagaraComponent;
class UCurveFloat;
class UNiagaraComponent;

UENUM(BlueprintType)
enum class EMysteryBoxState : uint8
{
    Idle,
    Spinning,
    WeaponOffered,
    TeddyBear,
    Cooldown
};

UCLASS()
class LASTSTANDLEGACY_API AMysteryBox : public AActor, public IInteractInterface
{
    GENERATED_BODY()

public:
    AMysteryBox();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> BoxBaseMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> TriggerBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> OfferedWeaponMesh;

    // IInteractInterface Implementation
    virtual void Interact(AHama* Player) override;
    virtual FString GetInteractMessage(AHama* InteractingPlayer) override;
    virtual bool CanInteract(AHama* InteractingPlayer) override;
    virtual bool Client_PreInteract(AHama* Player) override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(EditDefaultsOnly, Category = "MysteryBox|Settings")
    int32 MysteryBoxPrice = 950;

    UPROPERTY(EditDefaultsOnly, Category = "MysteryBox|Costs")
    int32 FireSalePrice = 10;

    UPROPERTY(EditDefaultsOnly, Category = "MysteryBox|Settings")
    float SpinDuration = 4.0f;

    UPROPERTY(EditDefaultsOnly, Category = "MysteryBox|Settings")
    float OfferDuration = 10.0f;

    UPROPERTY(ReplicatedUsing = OnRep_IsFireSaleActive)
    bool bIsFireSaleActive = false;

    bool bIsTemporaryFireSaleBox = false;

    UPROPERTY(EditDefaultsOnly, Category = "MysteryBox|TeddyBear")
    TObjectPtr<UStaticMesh> TeddyBearMesh;

    UPROPERTY(EditDefaultsOnly, Category = "MysteryBox|TeddyBear", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float TeddyBearChance = 0.15f;

    UPROPERTY(EditDefaultsOnly, Category = "MysteryBox|TeddyBear")
    int32 MinSpinsBeforeTeddy = 4;

    UPROPERTY(EditDefaultsOnly, Category = "MysteryBox|Settings")
    TArray<TSubclassOf<ABaseWeapon>> AvailableWeapons;

    UPROPERTY(ReplicatedUsing = OnRep_BoxState, BlueprintReadOnly, Category = "MysteryBox")
    EMysteryBoxState BoxState = EMysteryBoxState::Idle;

    UPROPERTY(ReplicatedUsing = OnRep_OfferedWeaponClass, BlueprintReadOnly, Category = "MysteryBox")
    TSubclassOf<ABaseWeapon> OfferedWeaponClass;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "MysteryBox")
    TObjectPtr<AHama> CurrentBuyer;

    UPROPERTY()
    TObjectPtr<AMysteryBoxSpawnPoint> CurrentSpawnPoint;

    UFUNCTION()
    void OnRep_BoxState();

    UFUNCTION()
    void OnRep_IsFireSaleActive();

    UFUNCTION()
    void OnRep_OfferedWeaponClass();

    void OpenMysteryBox(AHama* Player);
    void FinishSpin();
    void HandleTeddyBear();
    void RelocateBox();
    void ResetBox();
    void ResetToIdle();

    void CacheWeaponMeshes(); 
    void TryInitialSpawnSetup();

    TArray<TSubclassOf<ABaseWeapon>> GetFilteredWeaponsForPlayer(AHama* Player) const;

    void UpdateVisuals();
    void HandleBoxStateChanged();
    void CycleRandomWeaponMesh();

    UFUNCTION()
    void HandleRiseTimelineProgress(float Value);

private:
    FTimerHandle TimerHandle_Spin;
    FTimerHandle TimerHandle_OfferTimeout;
    FTimerHandle TimerHandle_ResetToIdle;
    FTimerHandle TimerHandle_TeddyBear;
    FTimerHandle TimerHandle_InitialSetup;
    FTimerHandle TimerHandle_VisualSpinCycle;
    FVector InitialOfferedMeshRelativeLocation;

    int32 CurrentSpinCount = 0;
    uint8 bPendingFireSaleDestroy : 1 = false;

public:
    void SetFireSaleActive(bool bActive);
    void HandleFireSaleEnd();
    bool IsTemporaryFireSaleBox() const { return bIsTemporaryFireSaleBox; }
    void SetIsTemporaryFireSaleBox(bool bTemp) { bIsTemporaryFireSaleBox = bTemp; }
    int32 GetCurrentPrice() const;
    AMysteryBoxSpawnPoint* GetCurrentSpawnPoint() const { return CurrentSpawnPoint; }
    void AssignSpawnPoint(AMysteryBoxSpawnPoint* NewSpawnPoint);

protected:
    // 🟢 timeline بۆ جوڵاندنی چەکەکە بە ئاڕاستەی Z
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UTimelineComponent* RiseTimelineComponent;

    // 🟢 Curve Float لە ئەدیتۆر دادەنرێت تا خێرایی بەرزبوونەوەی چەکەکە بپێوێت (0 تا 1)
    UPROPERTY(EditDefaultsOnly, Category = "Mystery Box|Effects")
    UCurveFloat* RiseCurve;

    UPROPERTY(EditDefaultsOnly, Category = "Mystery Box|Effects")
    float MaxRiseHeight = 80.0f;

    // 🟢 Audio & VFX
    UPROPERTY(EditDefaultsOnly, Category = "Mystery Box|Audio")
    TObjectPtr<USoundBase> SpinSound;

    UPROPERTY(EditDefaultsOnly, Category = "Door|Audio")
    TObjectPtr<USoundBase> RejectSound;

    UPROPERTY(EditDefaultsOnly, Category = "Mystery Box|Audio")
    TObjectPtr<USoundBase> TeddyBearSound;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UNiagaraComponent> LightBeamVFX;

    TArray<TObjectPtr<UStaticMesh>> CachedWeaponMeshes;


};