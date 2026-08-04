// Fill out your copyright notice in the Description page of Project Settings.


#include "FireSell.h"
#include "ZombieDirectorSubsystem.h"

void AFireSell::ActivatePowerUp(AHama* Player)
{
    if (HasAuthority())
    {
        if (UZombieDirectorSubsystem* Director = GetWorld()->GetSubsystem<UZombieDirectorSubsystem>())
        {
            Director->StartFireSale(FireSaleDuration);
        }
    }
}