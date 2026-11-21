#include "GobbleGumAction_AegisBurst.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "HealthComponent.h"
#include "GobbleGumComponent.h"
#include "Zombie.h"

UGobbleGumAction_AegisBurst::UGobbleGumAction_AegisBurst()
{
}

void UGobbleGumAction_AegisBurst::ActivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority() || bIsActive) return;

    UHealthComponent* HealthComp = CharacterOwner->FindComponentByClass<UHealthComponent>();
    UGobbleGumComponent* GumComp = CharacterOwner->FindComponentByClass<UGobbleGumComponent>();
    if (!HealthComp || !GumComp) return;

    Super::ActivateAction_Implementation(CharacterOwner);

    CachedOwner = CharacterOwner;
    CachedHealthComponent = HealthComp;
    CachedGumComponent = GumComp;

    HealthComp->OnPrePlayerDowned.BindUObject(this, &UGobbleGumAction_AegisBurst::HandlePrePlayerDowned);
    bEffectApplied = true;
}

bool UGobbleGumAction_AegisBurst::HandlePrePlayerDowned()
{
    if (!bIsActive) return false;

    ACharacter* OwnerChar = CachedOwner.Get();
    UHealthComponent* HealthComp = CachedHealthComponent.Get();
    UGobbleGumComponent* GumComp = CachedGumComponent.Get();
    if (!OwnerChar || !HealthComp || !GumComp) return false;

    // ١. گەڕاندنەوەی خوێن بۆ ١٠٠٪ و پێدانی ٣ چرکە بەرگری
    HealthComp->RestoreFullHealth();
    HealthComp->GrantInvulnerability(InvulnerabilityDuration);

    // ٢. دوورخستنەوەی زۆمبییەکانی دەوروبەر
    TriggerRadialBlast(OwnerChar);

    // ٣. خەرجکردنی شەحنەکە (دەبێت کۆتا دێڕ بێت چونکە DeactivateAction بانگ دەکات)
    GumComp->ConsumeEventTriggeredGum();

    return true;
}

void UGobbleGumAction_AegisBurst::TriggerRadialBlast(ACharacter* OwnerChar)
{
    UWorld* World = OwnerChar ? OwnerChar->GetWorld() : nullptr;
    if (!World) return;

    const FVector Origin = OwnerChar->GetActorLocation();
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AegisBurstOverlap), false, OwnerChar);

    const bool bHit = World->OverlapMultiByChannel(
        Overlaps,
        Origin,
        FQuat::Identity,
        ECC_Pawn,
        FCollisionShape::MakeSphere(BlastRadius),
        QueryParams
    );

    if (!bHit) return;

    // ڕێگری لە دووبارە حسابکردنی هەمان زۆمبی ئەگەر زیاتر لە کۆڵیژنێکی هەبێت
    TSet<AZombie*> AffectedZombies;
    for (const FOverlapResult& Result : Overlaps)
    {
        if (AZombie* Zombie = Cast<AZombie>(Result.GetActor()))
        {
            AffectedZombies.Add(Zombie);
        }
    }

    for (AZombie* Zombie : AffectedZombies)
    {
        if (!IsValid(Zombie)) continue;

        FVector PushDir = (Zombie->GetActorLocation() - Origin).GetSafeNormal2D();
        if (PushDir.IsNearlyZero())
        {
            PushDir = OwnerChar->GetActorForwardVector();
        }

        Zombie->LaunchCharacter((PushDir * KnockbackStrength) + FVector(0.0f, 0.0f, 220.0f), true, true);

        // ئەگەر فەنکشنی سڕکردن (Stun)ـت لەناو Zombie.h هەیە، لێرە بانگی بکە:
        // Zombie->StunZombie(StunDuration);
    }
}

void UGobbleGumAction_AegisBurst::DeactivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (bEffectApplied)
    {
        UHealthComponent* HealthComp = CachedHealthComponent.Get();
        if (!HealthComp && CharacterOwner)
        {
            HealthComp = CharacterOwner->FindComponentByClass<UHealthComponent>();
        }

        if (HealthComp)
        {
            HealthComp->OnPrePlayerDowned.Unbind();
        }

        bEffectApplied = false;
    }

    CachedOwner.Reset();
    CachedHealthComponent.Reset();
    CachedGumComponent.Reset();

    Super::DeactivateAction_Implementation(CharacterOwner);
}