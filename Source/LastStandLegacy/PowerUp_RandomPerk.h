#pragma once

#include "CoreMinimal.h"
#include "BasePowerUp.h"
#include "PowerUp_RandomPerk.generated.h"

class ABasePerk;
class AHama;

UCLASS(Blueprintable)
class LASTSTANDLEGACY_API APowerUp_RandomPerk : public ABasePowerUp
{
    GENERATED_BODY()

public:
    APowerUp_RandomPerk();

protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PowerUp|RandomPerk")
    TArray<TSubclassOf<ABasePerk>> PerkMachineClasses;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PowerUp|RandomPerk")
    bool bGiveToAllPlayers = true;

    virtual bool CanBeCollectedBy(AHama* Player) const override;
    virtual void ActivatePowerUp(AHama* Player) override;

private:
    bool PlayerHasAnyUnownedPerk(const AHama* TargetPlayer) const;
    void GrantRandomUnownedPerkToPlayer(AHama* TargetPlayer) const;
};