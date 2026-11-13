#include "GobbleGumAction_ReignDrops.h"
#include "GameFramework/Character.h"
#include "BasePowerUp.h"

UGobbleGumAction_ReignDrops::UGobbleGumAction_ReignDrops()
{
}

void UGobbleGumAction_ReignDrops::ActivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority()) return;

    Super::ActivateAction_Implementation(CharacterOwner);

    UWorld* World = CharacterOwner->GetWorld();
    if (!World || PowerUpClasses.Num() == 0)
    {
        NotifyActionFinished(CharacterOwner);
        return;
    }

    TArray<TSubclassOf<ABasePowerUp>> ValidClasses;
    ValidClasses.Reserve(PowerUpClasses.Num());
    for (const TSubclassOf<ABasePowerUp>& PowerUpClass : PowerUpClasses)
    {
        if (PowerUpClass)
        {
            ValidClasses.Add(PowerUpClass);
        }
    }

    const int32 TotalCount = ValidClasses.Num();
    if (TotalCount == 0)
    {
        NotifyActionFinished(CharacterOwner);
        return;
    }

    const FVector StartLocation = CharacterOwner->GetActorLocation();
    const FVector ForwardDir = CharacterOwner->GetActorForwardVector();

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(CharacterOwner);

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    const float AngleStep = (TotalCount > 1) ? (ArcAngleDegrees / static_cast<float>(TotalCount - 1)) : 0.f;
    const float StartAngle = (TotalCount > 1) ? (-ArcAngleDegrees * 0.5f) : 0.f;

    for (int32 i = 0; i < TotalCount; ++i)
    {
        const float CurrentAngle = StartAngle + (static_cast<float>(i) * AngleStep);
        const FVector SpawnDir = ForwardDir.RotateAngleAxis(CurrentAngle, FVector::UpVector);
        FVector TargetLocation = StartLocation + (SpawnDir * SpawnRadius);

        // پشکنینی دیوار بۆ هەر دانەیەک بە جیا تا هیچیان نەکەوێتە دیوی ئەودیوی دیوارەکە
        FHitResult HitResult;
        if (World->LineTraceSingleByChannel(HitResult, StartLocation, TargetLocation, ECC_Visibility, QueryParams))
        {
            TargetLocation = (HitResult.Distance > 40.f)
                ? (HitResult.Location - (SpawnDir * 30.f))
                : StartLocation;
        }

        World->SpawnActor<ABasePowerUp>(ValidClasses[i], TargetLocation, FRotator::ZeroRotator, SpawnParams);
    }

    NotifyActionFinished(CharacterOwner);
}