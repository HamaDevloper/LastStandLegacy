#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractInterface.h"
#include "PackAPunchMachine.generated.h"

class ABaseWeapon;
class AHama;
class UBoxComponent;

UENUM(BlueprintType)
enum class EPaPState : uint8
{
    Idle,
    Upgrading,
    ReadyForPickup
};

UCLASS(Abstract)
class LASTSTANDLEGACY_API APackAPunchMachine : public AActor, public IInteractInterface
{
    GENERATED_BODY()

public:
    APackAPunchMachine();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // --- IInteractInterface ---
    virtual bool CanInteract(AHama* InteractingPlayer) override;
    virtual void Interact(AHama* Player) override;
    virtual FString GetInteractMessage() override;

    // Must be called on the SERVER via Character/Interaction Component RPC
    void ExecuteServerInteraction(AHama* InteractingPlayer);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MachineMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> WeaponDisplaySocket;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
    TObjectPtr<USoundBase> RejectSound;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> InteractionVolume;

    // --- Replicated State ---
    UPROPERTY(ReplicatedUsing = OnRep_PapMachineState)
    EPaPState MachineState = EPaPState::Idle;

    UPROPERTY(ReplicatedUsing = OnRep_UpgradedWeaponClass)
    TSubclassOf<ABaseWeapon> UpgradedWeaponClass;

    UPROPERTY(Replicated)
    TObjectPtr<AHama> CurrentOwnerPlayer;

    // --- Visual Weapon Display Mesh (Local/Server Visual) ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USkeletalMeshComponent> DisplayWeaponMesh;

    // --- Settings ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pack-A-Punch|Settings")
    float UpgradeDuration = 5.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pack-A-Punch|Settings")
    float PickupExpirationTime = 15.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pack-A-Punch|Settings")
    int32 UpgradeCost = 5000;

    // --- Server Logic ---
    void StartUpgradeProcess(AHama* InteractingPlayer);
    void CompleteUpgradeProcess();
    void PickupUpgradedWeapon(AHama* InteractingPlayer);
    void HandlePickupExpired();
    void ResetMachineState();

    UFUNCTION()
    void OnRep_PapMachineState();

    UFUNCTION()
    void OnRep_UpgradedWeaponClass();

    void UpdateVisuals();

private:
    FTimerHandle UpgradeTimerHandle;
    FTimerHandle ExpirationTimerHandle;
};