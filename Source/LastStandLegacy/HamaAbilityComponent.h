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
    UPROPERTY(ReplicatedUsing = OnRep_IsGhost, BlueprintReadOnly, Category = "Abilities")
    bool bIsGhost = false;

    void SetAssignedAbility(EHamaAbilityType NewAbility);
    void AddPower(float Amount);
    UFUNCTION(Server, Reliable, BlueprintCallable)
    void Server_ActivateAbility();
    bool IsPowerFull() const;
    void FullPower();
    void StopAllAbilities();

    bool GetGhost() const { return bIsGhost; }

protected:
    UFUNCTION(BlueprintPure, Category = "Hama|Abilities")
    EHamaAbilityType GetCurrentAssignedAbility() const { return CurrentAssignedAbility; }

protected:
    UPROPERTY(BlueprintAssignable, Category = "Hama|Abilities")
    FOnPowerChangedSignature OnPowerChanged;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hama|Abilities")
    EHamaAbilityType CurrentAssignedAbility = EHamaAbilityType::None;

    
    UPROPERTY(ReplicatedUsing = OnRep_CurrentPower, VisibleAnywhere, BlueprintReadOnly, Category = "Hama|Abilities")
    float CurrentPower = 0.0f;

    UPROPERTY(EditAnywhere, Category = "Ability")
    float SphereRadius = 500.f;

    const float MaxPower = 100.0f;

    UFUNCTION()
    void OnRep_CurrentPower();

    void ActivateBulletStorm();
    void ActivateMedicalSupport();
    void ActivateGhostMode();
    void ActivateDecoy();

    UFUNCTION()
    void DeactivateGhostMode();
   
protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Abilities")
    float AbilityDuration = 10.0f;

    UFUNCTION()
    void OnRep_IsGhost();

private:
    FTimerHandle GhostTimerHandle;
};