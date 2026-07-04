#include "HealthComponent.h"
#include "Hama.h"
#include "HamaComponent.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"

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

void UHealthComponent::HandlePlayerDeath()
{
    if (!GetOwner()->HasAuthority()) return;
    if (!OwnerCharacter) return;
    if (OwnerCharacter->bIsDead) return;
  
        OwnerCharacter->HandleDeath();
        MaxHealth = 100.f;
        CurrentHealth = 100;

        APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());

        if (PC)
        {
            PC->UnPossess();

            AHama* PlayerToSpectate = nullptr;
            for (TActorIterator<AHama> ActorItr(GetWorld()); ActorItr; ++ActorItr)
            {
                AHama* OtherPlayer = *ActorItr;

                if (OtherPlayer && OtherPlayer != OwnerCharacter && !OtherPlayer->bIsDead)
                {
                    PlayerToSpectate = OtherPlayer;
                    break;
                }
            }

            PC->ChangeState(NAME_Spectating);
            PC->ClientGotoState(NAME_Spectating);

            if (PlayerToSpectate)
            {
                PC->SetViewTargetWithBlend(PlayerToSpectate, 0.5f);
            }
        }

        OwnerCharacter->ForceNetUpdate();
        OwnerCharacter->Destroy();
}