#include "GobbleGumAction_MaxAmmo.h"
#include "GameFramework/Character.h"
#include "BasePowerUp.h"

UGobbleGumAction_MaxAmmo::UGobbleGumAction_MaxAmmo()
{
}

void UGobbleGumAction_MaxAmmo::ActivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority()) return;

    Super::ActivateAction_Implementation(CharacterOwner);

    UWorld* World = CharacterOwner->GetWorld();
    if (!World || !MaxAmmoPowerUpClass)
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

    World->SpawnActor<ABasePowerUp>(MaxAmmoPowerUpClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);

    NotifyActionFinished(CharacterOwner);
}