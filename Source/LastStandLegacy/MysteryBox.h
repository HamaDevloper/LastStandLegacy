#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractInterface.h"
#include "MysteryBox.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class AHama;
class ABaseWeapon;
class AMysteryBoxSpawnPoint;

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
    virtual FString GetInteractMessage() override;
    virtual bool CanInteract(AHama* InteractingPlayer) override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(EditDefaultsOnly, Category = "MysteryBox|Settings")
    int32 MysteryBoxPrice = 950;

    UPROPERTY(EditDefaultsOnly, Category = "MysteryBox|Settings")
    float SpinDuration = 4.0f;

    UPROPERTY(EditDefaultsOnly, Category = "MysteryBox|Settings")
    float OfferDuration = 10.0f;

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

    UFUNCTION()
    void OnRep_BoxState();

    UFUNCTION()
    void OnRep_OfferedWeaponClass();

    void OpenMysteryBox(AHama* Player);
    void FinishSpin();
    void HandleTeddyBear();
    void RelocateBox();
    void ResetBox();
    void ResetToIdle();

    // ⚡ لۆجیکی فلتەرکردنی ئەو چەکانەی یاریزانەکە هەیەتی
    TArray<TSubclassOf<ABaseWeapon>> GetFilteredWeaponsForPlayer(AHama* Player) const;

    void UpdateVisuals();

    UPROPERTY()
    TObjectPtr<AMysteryBoxSpawnPoint> CurrentSpawnPoint;

private:
    FTimerHandle TimerHandle_Spin;
    FTimerHandle TimerHandle_OfferTimeout;
    FTimerHandle TimerHandle_ResetToIdle;
    FTimerHandle TimerHandle_TeddyBear;

    int32 CurrentSpinCount = 0;
};