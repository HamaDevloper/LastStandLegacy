// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HamaAbilityComponent.generated.h"

class AHama;
class ALastStandLegacyGameState;
class UZombieDirectorSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPowerChangedSignature, float, NewPower);

UENUM(BlueprintType)
enum class EHamaAbilityType : uint8
{
    None           UMETA(DisplayName = "None"),
    BulletStorm    UMETA(DisplayName = "BulletStorm"),
    MedicalSupport UMETA(DisplayName = "MedicalSupport"),
    GhostMode      UMETA(DisplayName = "GhostMode"),
    Blitz          UMETA(DisplayName = "Blitz"),
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

    UPROPERTY(ReplicatedUsing = OnRep_IsAbilityActive)
    bool bIsAbilityActive = false;

    void SetAssignedAbility(EHamaAbilityType NewAbility);
    void AddPower(float Amount);
    UFUNCTION(Server, Reliable, BlueprintCallable)
    void Server_ActivateAbility();
    bool IsPowerFull() const;
    void FullPower();
    void StopAllAbilities();

    bool GetGhost() const { return bIsGhost; }

    EHamaAbilityType GetAssignedAbility() const { return CurrentAssignedAbility; }

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

    void ResetPower();

    UFUNCTION()
    void OnRep_CurrentPower();

    UFUNCTION()
    void OnRep_IsAbilityActive();

    void ActivateBulletStorm();
    void ActivateMedicalSupport();
    void ActivateGhostMode();
    void ActivateBlitz();

    void StartAbilityCooldown(float Duration);
    void EndAbilityCooldown();

    UFUNCTION()
    void DeactivateGhostMode();

protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Abilities")
    float AbilityDuration = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Abilities")
    float BlitzAbilityDuration = 20.0f;

    UFUNCTION()
    void OnRep_IsGhost();

private:
    FTimerHandle GhostTimerHandle;
    FTimerHandle AbilityDurationTimerHandle;

    UPROPERTY()
    TObjectPtr<AHama> CachedOwner;

    UPROPERTY()
    mutable TObjectPtr<ALastStandLegacyGameState> CachedGameState;

    UPROPERTY()
    mutable TObjectPtr<UZombieDirectorSubsystem> CachedDirector;

    ALastStandLegacyGameState* GetGameState() const;

    UZombieDirectorSubsystem* GetZombieDirector() const;


public:
    bool CanActivateMedicalSupportLocal() const;
    bool IsAbilityActive() const;
};