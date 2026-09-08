#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ThrowableComponent.generated.h"

class UAnimMontage;
class AHama;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnThrowableCountChanged, int32, NewCount);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LASTSTANDLEGACY_API UThrowableComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UThrowableComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintCallable, Category = "Throwable")
    void RequestThrow();

protected:
    virtual void BeginPlay() override;

    UFUNCTION(Server, Reliable)
    void Server_Throw(FVector_NetQuantize10 LaunchDirection);

    UFUNCTION()
    void OnRep_ThrowableCount();

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Config")
    TSubclassOf<AActor> ThrowableClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Config")
    TObjectPtr<UAnimMontage> ThrowMontage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Config")
    float ThrowImpulseStrength = 1200.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Config")
    FName HandSocketName = TEXT("Muzzle_R_Hand");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Config")
    int32 MaxThrowableCount = 3;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throwable|Config")
    float ThrowCooldown = 1.0f;

    UPROPERTY(ReplicatedUsing = OnRep_ThrowableCount, BlueprintReadOnly, Category = "Throwable|State")
    int32 CurrentThrowableCount = 3;

    UPROPERTY(BlueprintAssignable, Category = "Throwable|Events")
    FOnThrowableCountChanged OnThrowableCountChanged;

private:
    UPROPERTY()
    TObjectPtr<AHama> CharacterOwner;

    float LastThrowTime = 0.0f;

    void GetSafeSpawnLocation(const FVector& StartLoc, const FVector& TargetLoc, FVector& OutSpawnLoc) const;
};