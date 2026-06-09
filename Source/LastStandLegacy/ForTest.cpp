// Fill out your copyright notice in the Description page of Project Settings.


#include "ForTest.h"

// Sets default values
AForTest::AForTest()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

void AForTest::BeginPlay()
{
	Super::BeginPlay();

	// ١. دروستکردنی لیستەکە بە ناوی SessionsList
	TArray<FPlayerSession> SessionsList = {
		// یاریزانی یەکەم
		{ TEXT("Hama"), EPlayerRole::Developer, 150 },

		// یاریزانی دووەم
		{ TEXT("Ahmad"), EPlayerRole::VIP, 45 },

		// یاریزانی سێیەم
		{ TEXT("Sako"), EPlayerRole::Guest, 12 }
	};

	// ---- لێرەوە دەتوانیت دەست بکەیت بە نووسینی لامبداکەت ----
	auto FindAndVerifySession = [this, &SessionsList](const FString InputName) -> FPlayerSession*
		{
			
		};


}

// Called every frame
void AForTest::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

