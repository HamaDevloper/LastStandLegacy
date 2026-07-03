// Fill out your copyright notice in the Description page of Project Settings.


#include "PerkSpawnPoint.h"

// Sets default values
APerkSpawnPoint::APerkSpawnPoint()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void APerkSpawnPoint::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void APerkSpawnPoint::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

