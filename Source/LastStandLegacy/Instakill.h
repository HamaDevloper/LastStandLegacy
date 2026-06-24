// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Instakill.generated.h"

class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class LASTSTANDLEGACY_API AInstakill : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AInstakill();

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* CollisionSphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* MeshComp;

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UPROPERTY(EditAnywhere, Category = "PowerUp")
    float Duration = 30.0f;

public:	
	virtual void Tick(float DeltaTime) override;

};
