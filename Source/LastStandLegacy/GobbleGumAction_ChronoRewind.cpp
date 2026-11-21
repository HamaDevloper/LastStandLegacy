#include "GobbleGumAction_ChronoRewind.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "HealthComponent.h"
#include "GobbleGumComponent.h"
#include "BaseWeapon.h"
#include "Zombie.h"
#include "Hama.h"

UGobbleGumAction_ChronoRewind::UGobbleGumAction_ChronoRewind()
{
}

void UGobbleGumAction_ChronoRewind::ActivateAction_Implementation(ACharacter* CharacterOwner)
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority() || bIsActive) return;

    UHealthComponent* HealthComp = CharacterOwner->FindComponentByClass<UHealthComponent>();
    UGobbleGumComponent* GumComp = CharacterOwner->FindComponentByClass<UGobbleGumComponent>();
    if (!HealthComp || !GumComp) return;

    Super::ActivateAction_Implementation(CharacterOwner);

    CachedOwner = CharacterOwner;
    CachedHealthComponent = HealthComp;
    CachedGumComponent = GumComp;

    Snapshots.Empty(MaxSnapshots);

    RecordSnapshot();

    if (UWorld* World = CharacterOwner->GetWorld())
    {
        World->GetTimerManager().ClearTimer(TimerHandle_Record);
        World->GetTimerManager().SetTimer(
            TimerHandle_Record,
            this,
            &UGobbleGumAction_ChronoRewind::RecordSnapshot,
            RecordInterval,
            true
        );
    }
}

void UGobbleGumAction_ChronoRewind::TriggerManualAction()
{
    TriggerRewind();
}

void UGobbleGumAction_ChronoRewind::RecordSnapshot()
{
    if (!bIsActive) return;

    ACharacter* OwnerChar = CachedOwner.Get();
    UHealthComponent* HealthComp = CachedHealthComponent.Get();
    AHama* HamaChar = Cast<AHama>(OwnerChar);
    if (!OwnerChar || !HealthComp || !HamaChar) return;

    if (HealthComp->bIsDowned || HamaChar->bIsDead) return;

    FChronoSnapshot Snap;
    Snap.Location = OwnerChar->GetActorLocation();
    Snap.ControlRotation = OwnerChar->GetControlRotation();
    Snap.Health = HealthComp->CurrentHealth;

    if (HamaChar->CurrentWeapon)
    {
        Snap.WeaponActor = HamaChar->CurrentWeapon;
        Snap.ClipAmmo = HamaChar->CurrentWeapon->GetCurrentAmmo();
        Snap.ReserveAmmo = HamaChar->CurrentWeapon->GetReserveAmmo();
    }

    Snapshots.Add(Snap);
    if (Snapshots.Num() > MaxSnapshots)
    {
        Snapshots.RemoveAt(0);
    }
}

void UGobbleGumAction_ChronoRewind::TriggerRewind()
{
    if (!bIsActive || Snapshots.Num() == 0) return;

    ACharacter* OwnerChar = CachedOwner.Get();
    UHealthComponent* HealthComp = CachedHealthComponent.Get();
    UGobbleGumComponent* GumComp = CachedGumComponent.Get();
    AHama* HamaChar = Cast<AHama>(OwnerChar);
    if (!OwnerChar || !OwnerChar->HasAuthority() || !HealthComp || !GumComp || !HamaChar) return;

    if (HealthComp->bIsDowned || HamaChar->bIsDead) return;

    // کۆنترین سناپشۆت (٤ چرکە پێش ئێستا)
    const FChronoSnapshot TargetSnap = Snapshots[0];
    const FVector CurrentLocation = OwnerChar->GetActorLocation();

    // ١. پاڵنانی زۆمبییەکان لە هەردوو شوێنەکە (شوێنی ئێستا و شوێنی گەڕانەوە تا زۆمبی ڕێگری لە Teleport نەکات)
    KnockbackNearbyZombies(OwnerChar, CurrentLocation);
    KnockbackNearbyZombies(OwnerChar, TargetSnap.Location);

    // ٢. گەڕاندنەوەی یاریزان بۆ شوێن و ئاڕاستەی کامێرای ٤ چرکە پێش ئێستا
    const FRotator YawOnlyRot(0.0f, TargetSnap.ControlRotation.Yaw, 0.0f);
    OwnerChar->TeleportTo(TargetSnap.Location, YawOnlyRot, false, false);

    if (AController* PC = OwnerChar->GetController())
    {
        PC->SetControlRotation(TargetSnap.ControlRotation);
        PC->ClientSetRotation(TargetSnap.ControlRotation);
    }

    // ٣. گەڕاندنەوەی خوێن (هەرگیز خوێنی یاریزان کەم ناکەینەوە ئەگەر پڕ بووبێتەوە)
    const float RestoredHealth = FMath::Max(HealthComp->CurrentHealth, TargetSnap.Health);
    HealthComp->CurrentHealth = FMath::Clamp(RestoredHealth, 1.0f, HealthComp->MaxHealth);
    MARK_PROPERTY_DIRTY_FROM_NAME(UHealthComponent, CurrentHealth, HealthComp);

    // ٤. گەڕاندنەوەی فیشەک ئەگەر هێشتا هەمان چەکی بەدەستەوە بێت
    if (HamaChar->CurrentWeapon && TargetSnap.WeaponActor.Get() == HamaChar->CurrentWeapon)
    {
        HamaChar->CurrentWeapon->RestoreAmmoFromRewind(TargetSnap.ClipAmmo, TargetSnap.ReserveAmmo);
    }

    // ٥. خەرجکردنی شەحنی گەمەکە
    GumComp->ConsumeEventTriggeredGum();
}

void UGobbleGumAction_ChronoRewind::KnockbackNearbyZombies(ACharacter* OwnerChar, const FVector& Origin)
{
    UWorld* World = OwnerChar ? OwnerChar->GetWorld() : nullptr;
    if (!World) return;

    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ChronoRewindBlast), false, OwnerChar);

    if (!World->OverlapMultiByChannel(
        Overlaps,
        Origin,
        FQuat::Identity,
        ECC_Pawn,
        FCollisionShape::MakeSphere(DepartureBlastRadius),
        QueryParams))
    {
        return;
    }

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

        Zombie->LaunchCharacter((PushDir * DepartureKnockback) + FVector(0.0f, 0.0f, 200.0f), true, true);
    }
}

void UGobbleGumAction_ChronoRewind::DeactivateAction_Implementation(ACharacter* CharacterOwner)
{
    UWorld* World = CharacterOwner
        ? CharacterOwner->GetWorld()
        : (CachedOwner.IsValid() ? CachedOwner->GetWorld() : nullptr);

    if (World)
    {
        World->GetTimerManager().ClearTimer(TimerHandle_Record);
    }

    Snapshots.Empty();
    CachedOwner.Reset();
    CachedHealthComponent.Reset();
    CachedGumComponent.Reset();

    Super::DeactivateAction_Implementation(CharacterOwner);
}