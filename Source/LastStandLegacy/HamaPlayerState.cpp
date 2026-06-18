// Fill out your copyright notice in the Description page of Project Settings.


#include "HamaPlayerState.h"
#include "Net/UnrealNetwork.h"

AHamaPlayerState::AHamaPlayerState()
{
    Points = 0;
}

void AHamaPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AHamaPlayerState, Points);
}

void AHamaPlayerState::AddPoints(int32 Amount)
{
    if (HasAuthority())
    {
        Points += Amount;
    }
}

void AHamaPlayerState::OnRep_Points()
{
}