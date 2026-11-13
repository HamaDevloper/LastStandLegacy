#include "GobbleGumAction_SpawnPowerUp.h"
#include "GameFramework/Character.h"
#include "BasePowerUp.h"

UGobbleGumAction_SpawnPowerUp::UGobbleGumAction_SpawnPowerUp()
{
}

void UGobbleGumAction_SpawnPowerUp::ActivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority()) return;

    Super::ActivateAction_Implementation(CharacterOwner);

    UWorld* World = CharacterOwner->GetWorld();
    if (!World || !PowerUpClass)
    {
        NotifyActionFinished(CharacterOwner);
        return;
    }

    const FVector StartLocation = CharacterOwner->GetActorLocation();
    const FVector ForwardDir = CharacterOwner->GetActorForwardVector();
    FVector SpawnLocation = StartLocation + (ForwardDir * SpawnForwardDistance);

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(CharacterOwner);

    if (World->LineTraceSingleByChannel(HitResult, StartLocation, SpawnLocation, ECC_Visibility, QueryParams))
    {
        SpawnLocation = (HitResult.Distance > 40.f)
            ? (HitResult.Location - (ForwardDir * 30.f))
            : StartLocation;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    World->SpawnActor<ABasePowerUp>(PowerUpClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);

    NotifyActionFinished(CharacterOwner);
}