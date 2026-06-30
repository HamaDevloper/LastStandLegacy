#include "HealthComponent.h"
#include "Hama.h"
#include "HamaComponent.h"
#include "Net/UnrealNetwork.h"

UHealthComponent::UHealthComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UHealthComponent::BeginPlay()
{
    Super::BeginPlay();

    OwnerCharacter = Cast<AHama>(GetOwner());

    if (OwnerCharacter)
    {
        OwnerComponent = OwnerCharacter->FindComponentByClass<UHamaComponent>();
    }
}

void UHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION(UHealthComponent, CurrentHealth, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UHealthComponent, MaxHealth, COND_OwnerOnly);
}

void UHealthComponent::UpgradeHealth(float Amount)
{
    if (!OwnerCharacter || !OwnerCharacter->HasAuthority()) return;

    MaxHealth = Amount;
    OwnerCharacter->ForceNetUpdate();

    if (!GetWorld()->GetTimerManager().IsTimerActive(RegenerateHealthTimer))
    {
        GetWorld()->GetTimerManager().SetTimer(RegenerateHealthTimer, this, &UHealthComponent::RegenerateHealth, HealthTickGenerate, true, HealthGenerateDelay);
    }
}

void UHealthComponent::GetDamage(float Amount)
{
    if (!OwnerCharacter || !OwnerCharacter->HasAuthority()) return;

    GetWorld()->GetTimerManager().ClearTimer(RegenerateHealthTimer);

    CurrentHealth = FMath::Clamp(CurrentHealth - Amount, 0.f, MaxHealth);
    OwnerCharacter->ForceNetUpdate();

    if (CurrentHealth <= 0.f)
    {
        DownPlayer();
    }
    else
    {
        GetWorld()->GetTimerManager().SetTimer(RegenerateHealthTimer, this, &UHealthComponent::RegenerateHealth, HealthTickGenerate, true, HealthGenerateDelay);
    }
}

void UHealthComponent::RegenerateHealth()
{
    float HealAmountPerTick = MaxHealth / 20.0f;

    CurrentHealth += HealAmountPerTick;

    if (CurrentHealth >= MaxHealth)
    {
        CurrentHealth = MaxHealth;
        GetWorld()->GetTimerManager().ClearTimer(RegenerateHealthTimer);
    }

    if (OwnerCharacter)
    {
        OwnerCharacter->ForceNetUpdate();
    }
}

void UHealthComponent::DownPlayer()
{
    // لێرەدا فەنکشنەکانی داونبوون بانگ دەکەین
}

void UHealthComponent::Revive()
{
}