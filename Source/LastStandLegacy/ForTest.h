// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ForTest.generated.h"

enum class EPlayerRole : uint8
{
	Guest,
	VIP,
	Developer
};

struct FPlayerSession
{
	FString PlayerName;
	EPlayerRole PlayerRole;
	int32 killCount;
};

UCLASS()
class LASTSTANDLEGACY_API AForTest : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AForTest();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	float ForTestfloat = 10.f;

};
