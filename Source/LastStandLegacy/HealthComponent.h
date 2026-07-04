// Fill out your copyright notice in the Description page of Project Settings.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

class AHama;
class UHamaComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LASTSTANDLEGACY_API UHealthComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UHealthComponent();

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Health")
    float MaxHealth = 100.f;

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Health")
    float CurrentHealth = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
    float HealthGenerateDelay = 3.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
    float HealthTickGenerate = 0.1f;

public:
    void GetDamage(float Amount);
    void UpgradeHealth(float Amount);

protected:
    void DownPlayer();
    void RegenerateHealth();
    void Revive();
    void HandlePlayerDeath();

private:
    UPROPERTY()
    TObjectPtr<AHama> OwnerCharacter;
    UPROPERTY()
    TObjectPtr<UHamaComponent> OwnerComponent;
    FTimerHandle RegenerateHealthTimer;

    FTimerHandle ReviveTimerHandle;
    FTimerHandle DownTimerHandle;
};