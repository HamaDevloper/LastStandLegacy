// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HamaAbilityComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPowerChangedSignature, float, NewPower);

UENUM(BlueprintType)
enum class EHamaAbilityType : uint8
{
    None           UMETA(DisplayName = "None"),
    BulletStorm    UMETA(DisplayName = "BulletStorm"),
    MedicalSupport UMETA(DisplayName = "MedicalSupport"),
    GhostMode      UMETA(DisplayName = "GhostMode"),
    Decoy          UMETA(DisplayName = "Decoy"),
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LASTSTANDLEGACY_API UHamaAbilityComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHamaAbilityComponent();

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
    UFUNCTION(BlueprintImplementableEvent, Category = "Hama|Abilities")
    void OnRoleAssigned_BP(EHamaAbilityType NewRole);

    void SetAssignedAbility(EHamaAbilityType NewAbility);
    void AddPower(float Amount);
    UFUNCTION(Server, Reliable, BlueprintCallable)
    void Server_ActivateAbility();
protected:
    UFUNCTION(BlueprintPure, Category = "Hama|Abilities")
    FORCEINLINE EHamaAbilityType GetCurrentAssignedAbility() const { return CurrentAssignedAbility; }

protected:
    UPROPERTY(BlueprintAssignable, Category = "Hama|Abilities")
    FOnPowerChangedSignature OnPowerChanged;

    UPROPERTY(ReplicatedUsing = OnRep_CurrentAssignedAbility, VisibleAnywhere, BlueprintReadOnly, Category = "Hama|Abilities")
    EHamaAbilityType CurrentAssignedAbility = EHamaAbilityType::None;

    
    UPROPERTY(ReplicatedUsing = OnRep_CurrentPower, VisibleAnywhere, BlueprintReadOnly, Category = "Hama|Abilities")
    float CurrentPower = 0.0f;

    const float MaxPower = 100.0f;

    UFUNCTION()
    void OnRep_CurrentAssignedAbility();

    UFUNCTION()
    void OnRep_CurrentPower();

    void ActivateBulletStorm();
    void ActivateMedicalSupport();
    void ActivateGhostMode();
    void ActivateDecoy();

    void DeactivateBulletStorm();
   
protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Abilities")
    float BulletStormDuration = 10.0f;

private:
    FTimerHandle BulletStormTimerHandle;
};