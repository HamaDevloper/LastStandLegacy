#pragma once

#include "CoreMinimal.h"
#include "GobbleGumAction.h"
#include "TimerManager.h"
#include "GobbleGumAction_ChronoRewind.generated.h"

class UHealthComponent;
class UGobbleGumComponent;
class ABaseWeapon;

USTRUCT()
struct FChronoSnapshot
{
    GENERATED_BODY()

    FVector Location = FVector::ZeroVector;
    FRotator ControlRotation = FRotator::ZeroRotator;
    float Health = 100.0f;

    TWeakObjectPtr<ABaseWeapon> WeaponActor;
    int32 ClipAmmo = 0;
    int32 ReserveAmmo = 0;
};

/**
 * Chrono Rewind: شوێن، خوێن و فیشەکی یاریزان دەگەڕێنێتەوە بۆ ٤ چرکە پێش ئێستا
 * و زۆمبییەکانی شوێنی پێشووی پاڵ دەنێت.
 */
UCLASS(Blueprintable, meta = (DisplayName = "GobbleGum Action: Chrono Rewind"))
class LASTSTANDLEGACY_API UGobbleGumAction_ChronoRewind : public UGobbleGumAction
{
    GENERATED_BODY()

public:
    UGobbleGumAction_ChronoRewind();

    virtual void ActivateAction_Implementation(ACharacter* CharacterOwner) override;
    virtual void DeactivateAction_Implementation(ACharacter* CharacterOwner) override;
    virtual void TriggerManualAction() override;

    // ئەم فەنکشنە کاتێک یاریزان دوگمەی بەکارهێنانی گۆبڵگەم دادەگرێت لەسەر سێرڤەر بانگ دەکرێت
    UFUNCTION(BlueprintCallable, Category = "GobbleGum|ChronoRewind")
    void TriggerRewind();

protected:
    // هەر چەند چرکە جارێک دۆخی یاریزان تۆمار بکرێت
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|ChronoRewind", meta = (ClampMin = "0.25"))
    float RecordInterval = 1.0f;

    // ٥ خانە بە ماوەی ١ چرکە = گەڕانەوە بۆ ٤ چرکە پێش ئێستا
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|ChronoRewind", meta = (ClampMin = "2", ClampMax = "10"))
    int32 MaxSnapshots = 5;

    // بازنەی پاڵنانی زۆمبییەکان لەو شوێنەی جێی دەهێڵیت
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|ChronoRewind", meta = (ClampMin = "50.0"))
    float DepartureBlastRadius = 350.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GobbleGum|ChronoRewind", meta = (ClampMin = "0.0"))
    float DepartureKnockback = 800.0f;

private:
    TWeakObjectPtr<ACharacter> CachedOwner;
    TWeakObjectPtr<UHealthComponent> CachedHealthComponent;
    TWeakObjectPtr<UGobbleGumComponent> CachedGumComponent;

    TArray<FChronoSnapshot> Snapshots;
    FTimerHandle TimerHandle_Record;

    void RecordSnapshot();
    void KnockbackNearbyZombies(ACharacter* OwnerChar, const FVector& Origin);
};