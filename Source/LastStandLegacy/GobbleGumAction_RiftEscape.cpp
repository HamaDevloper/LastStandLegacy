#include "GobbleGumAction_RiftEscape.h"
#include "RiftAnchor.h"
#include "Hama.h"
#include "HealthComponent.h"
#include "ZombieDirectorSubsystem.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"

UGobbleGumAction_RiftEscape::UGobbleGumAction_RiftEscape()
{
}

void UGobbleGumAction_RiftEscape::ActivateAction_Implementation(ACharacter* CharacterOwner)
{
    AHama* HamaChar = Cast<AHama>(CharacterOwner);
    if (!HamaChar || !HamaChar->HasAuthority() || bIsActive || HamaChar->IsDowned() || HamaChar->bIsDead) return;

    UWorld* World = HamaChar->GetWorld();
    if (!World) return;

    UZombieDirectorSubsystem* Director = World->GetSubsystem<UZombieDirectorSubsystem>();

    const FVector CurrentLocation = HamaChar->GetActorLocation();
    const float MinDistSq = FMath::Square(MinTeleportDistance);

    TArray<ARiftAnchor*> FarAnchors;
    TArray<ARiftAnchor*> FallbackAnchors;

    for (TActorIterator<ARiftAnchor> It(World); It; ++It)
    {
        ARiftAnchor* Anchor = *It;
        if (!IsValid(Anchor)) continue;

        bool bIsUnlocked = bAllowLockedZones || Anchor->bStartsUnlocked || (Anchor->ZoneID <= 0);
        if (!bIsUnlocked && Director)
        {
            bIsUnlocked = Director->IsZoneUnlocked(Anchor->ZoneID);
        }

        if (!bIsUnlocked) continue;

        const float DistSq = FVector::DistSquared(CurrentLocation, Anchor->GetActorLocation());
        if (DistSq >= MinDistSq)
        {
            FarAnchors.Add(Anchor);
        }
        else if (DistSq > FMath::Square(200.0f)) 
        {
            FallbackAnchors.Add(Anchor);
        }
    }

    if (FarAnchors.Num() == 0 && FallbackAnchors.Num() == 0) return;

    if (HamaChar->IsSprinting())
    {
        HamaChar->StopSprint();
    }

    if (UCharacterMovementComponent* MoveComp = HamaChar->GetCharacterMovement())
    {
        MoveComp->StopMovementImmediately();
    }

    // هەوڵدان بۆ گواستنەوە بۆ خاڵێکی هەڕەمەکی؛ ئەگەر زۆمبی لەسەر خاڵی یەکەم وەستابوو، یەکسەر خاڵێکی تر تاقی دەکاتەوە
    auto TryTeleportToPool = [&](TArray<ARiftAnchor*>& Pool) -> ARiftAnchor*
        {
            while (Pool.Num() > 0)
            {
                const int32 RandIdx = FMath::RandRange(0, Pool.Num() - 1);
                ARiftAnchor* Candidate = Pool[RandIdx];
                const FRotator YawOnlyRot(0.0f, Candidate->GetActorRotation().Yaw, 0.0f);

                if (HamaChar->TeleportTo(Candidate->GetActorLocation(), YawOnlyRot, false, false))
                {
                    return Candidate;
                }

                Pool.RemoveAtSwap(RandIdx);
            }
            return nullptr;
        };

    ARiftAnchor* ChosenAnchor = TryTeleportToPool(FarAnchors);
    if (!ChosenAnchor)
    {
        ChosenAnchor = TryTeleportToPool(FallbackAnchors);
    }

    if (!ChosenAnchor) return;

    Super::ActivateAction_Implementation(CharacterOwner);

    const FRotator FinalYawRot(0.0f, ChosenAnchor->GetActorRotation().Yaw, 0.0f);
    if (AController* PC = HamaChar->GetController())
    {
        PC->SetControlRotation(FinalYawRot);
        PC->ClientSetRotation(FinalYawRot);
    }

    DeactivateAction(CharacterOwner);
}