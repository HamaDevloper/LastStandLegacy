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
    Brawler        UMETA(DisplayName = "Brawler")
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

    UPROPERTY(ReplicatedUsing = OnRep_IsAbilityActive, BlueprintReadOnly, Category = "Abilities")
    bool bIsAbilityActive = false;

    UPROPERTY(ReplicatedUsing = OnRep_IsBrawler, BlueprintReadOnly, Category = "Abilities")
    bool bIsBrawler = false;

    void SetAssignedAbility(EHamaAbilityType NewAbility);
    void AddPower(float Amount);

    UFUNCTION(Server, Reliable, BlueprintCallable)
    void Server_ActivateAbility();

    bool IsPowerFull() const;
    void FullPower();
    void StopAllAbilities();

    bool IsAbilityActive() const { return bIsAbilityActive; }
    bool GetGhost() const { return bIsGhost; }
    bool IsBrawlerActive() const { return bIsBrawler; }

    float GetBrawlerDamage() const { return BrawlerDamage; }
    float GetBrawlerCooldown() const { return BrawlerCooldown; }
    float GetBrawlerRange() const { return BrawlerRange; }
    float GetBrawlerRadius() const { return BrawlerRadius; }

    EHamaAbilityType GetAssignedAbility() const { return CurrentAssignedAbility; }

protected:
    UFUNCTION(BlueprintPure, Category = "Hama|Abilities")
    EHamaAbilityType GetCurrentAssignedAbility() const { return CurrentAssignedAbility; }

    UPROPERTY(BlueprintAssignable, Category = "Hama|Abilities")
    FOnPowerChangedSignature OnPowerChanged;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hama|Abilities")
    EHamaAbilityType CurrentAssignedAbility = EHamaAbilityType::None;

    UPROPERTY(ReplicatedUsing = OnRep_CurrentPower, VisibleAnywhere, BlueprintReadOnly, Category = "Hama|Abilities")
    float CurrentPower = 0.0f;

    UPROPERTY(EditAnywhere, Category = "Hama|Abilities")
    float SphereRadius = 500.f;

    const float MaxPower = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Abilities")
    float AbilityDuration = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hama|Abilities|Brawler")
    float BrawlerAbilityDuration = 20.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hama|Abilities|Brawler")
    float BrawlerDamage = 650.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hama|Abilities|Brawler")
    float BrawlerCooldown = 0.4f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hama|Abilities|Brawler")
    float BrawlerRange = 200.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hama|Abilities|Brawler")
    float BrawlerRadius = 55.0f;

    void ResetPower();
    void StartAbilityCooldown(float Duration);
    void EndAbilityCooldown();

    void ActivateBulletStorm();
    void ActivateMedicalSupport();
    void ActivateGhostMode();
    void ActivateBrawler();

    UFUNCTION()
    void DeactivateGhostMode();

    UFUNCTION()
    void DeactivateBrawler();

    UFUNCTION()
    void OnRep_CurrentPower();

    UFUNCTION()
    void OnRep_IsAbilityActive();

    UFUNCTION()
    void OnRep_IsGhost();

    UFUNCTION()
    void OnRep_IsBrawler();

private:
    FTimerHandle AbilityDurationTimerHandle;

    UPROPERTY()
    TObjectPtr<AHama> CachedOwner;

    UPROPERTY()
    mutable TObjectPtr<ALastStandLegacyGameState> CachedGameState;

    UPROPERTY()
    mutable TObjectPtr<UZombieDirectorSubsystem> CachedDirector;

    ALastStandLegacyGameState* GetGameState() const;
    UZombieDirectorSubsystem* GetZombieDirector() const;
};