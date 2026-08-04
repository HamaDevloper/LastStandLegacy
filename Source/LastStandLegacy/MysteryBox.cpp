#include "MysteryBox.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Hama.h"
#include "BaseWeapon.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "HamaPlayerState.h"
#include "MysteryBoxSpawnPoint.h"
#include "ZombieDirectorSubsystem.h"

AMysteryBox::AMysteryBox()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    SetReplicatingMovement(true);
    NetDormancy = DORM_DormantAll;

    SetNetUpdateFrequency(10.f);
    SetMinNetUpdateFrequency(2.f);

    BoxBaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
    RootComponent = BoxBaseMesh;
    BoxBaseMesh->SetMobility(EComponentMobility::Movable);
    BoxBaseMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    BoxBaseMesh->PrimaryComponentTick.bCanEverTick = false;

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BaseMeshBox"));
    TriggerBox->SetupAttachment(BoxBaseMesh);
    TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
    TriggerBox->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    TriggerBox->PrimaryComponentTick.bCanEverTick = false;

    OfferedWeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OfferedWeaponMesh"));
    OfferedWeaponMesh->SetupAttachment(BoxBaseMesh);
    OfferedWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    OfferedWeaponMesh->SetVisibility(false);
    OfferedWeaponMesh->PrimaryComponentTick.bCanEverTick = false;
}

void AMysteryBox::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        FTimerHandle InitialSetupHandle;
        GetWorldTimerManager().SetTimer(InitialSetupHandle, [this]()
            {
                if (CurrentSpawnPoint != nullptr) return;

                if (UZombieDirectorSubsystem* Director = GetWorld()->GetSubsystem<UZombieDirectorSubsystem>())
                {
                    AMysteryBoxSpawnPoint* InitialPoint = Director->GetRandomFreeMysteryBoxPoint(nullptr);
                    if (InitialPoint)
                    {
                        CurrentSpawnPoint = InitialPoint;
                        CurrentSpawnPoint->SetOccupied(true);
                        SetActorTransform(CurrentSpawnPoint->GetActorTransform());

                        FlushNetDormancy();
                        ForceNetUpdate();
                    }
                }
            }, 0.2f, false);
    }
}

void AMysteryBox::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearAllTimersForObject(this);
    Super::EndPlay(EndPlayReason);
}

void AMysteryBox::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Param;
    Param.bIsPushBased = true;

    DOREPLIFETIME_WITH_PARAMS_FAST(AMysteryBox, BoxState, Param);
    DOREPLIFETIME_WITH_PARAMS_FAST(AMysteryBox, OfferedWeaponClass, Param);
    DOREPLIFETIME_WITH_PARAMS_FAST(AMysteryBox, CurrentBuyer, Param);
    DOREPLIFETIME_WITH_PARAMS_FAST(AMysteryBox, bIsFireSaleActive, Param);
}

int32 AMysteryBox::GetCurrentPrice() const
{
    return bIsFireSaleActive ? FireSalePrice : MysteryBoxPrice;
}

bool AMysteryBox::CanInteract(AHama* InteractingPlayer)
{
    if (!InteractingPlayer) return false;

    if (InteractingPlayer->IsDowned() || InteractingPlayer->bIsDeathMachineActive)
    {
        return false;
    }

    if (BoxState == EMysteryBoxState::Idle)
    {
        AHamaPlayerState* PS = InteractingPlayer->GetPlayerState<AHamaPlayerState>();
        return PS && PS->GetPoints() >= GetCurrentPrice();
    }
    else if (BoxState == EMysteryBoxState::WeaponOffered)
    {
        return InteractingPlayer == CurrentBuyer;
    }

    return false;
}

void AMysteryBox::Interact(AHama* Player)
{
    if (!HasAuthority() || !Player || !CanInteract(Player)) return;

    const int32 Cost = GetCurrentPrice();

    if (BoxState == EMysteryBoxState::Idle)
    {
        AHamaPlayerState* PS = Player->GetPlayerState<AHamaPlayerState>();
        if (PS && PS->GetPoints() >= Cost)
        {
            PS->RemovePoints(Cost);
            OpenMysteryBox(Player);
        }
    }
    else if (BoxState == EMysteryBoxState::WeaponOffered && Player == CurrentBuyer)
    {
        if (OfferedWeaponClass)
        {
            Player->GiveWeapon(OfferedWeaponClass);
        }

        GetWorldTimerManager().ClearTimer(TimerHandle_OfferTimeout);
        ResetBox();
    }
}

FString AMysteryBox::GetInteractMessage()
{
    if (BoxState == EMysteryBoxState::Idle)
    {
        return FString::Printf(TEXT("Press F to Open MysteryBox [Cost %d]"), GetCurrentPrice());
    }
    else if (BoxState == EMysteryBoxState::WeaponOffered)
    {
        return TEXT("Press F to Take Weapon");
    }

    return TEXT("");
}

void AMysteryBox::OpenMysteryBox(AHama* Player)
{
    CurrentBuyer = Player;
    CurrentSpinCount++;

    MARK_PROPERTY_DIRTY_FROM_NAME(AMysteryBox, CurrentBuyer, this);

    BoxState = EMysteryBoxState::Spinning;
    MARK_PROPERTY_DIRTY_FROM_NAME(AMysteryBox, BoxState, this);
    UpdateVisuals();

    FlushNetDormancy();
    ForceNetUpdate();

    GetWorldTimerManager().SetTimer(TimerHandle_Spin, this, &AMysteryBox::FinishSpin, SpinDuration, false);
}

TArray<TSubclassOf<ABaseWeapon>> AMysteryBox::GetFilteredWeaponsForPlayer(AHama* Player) const
{
    TArray<TSubclassOf<ABaseWeapon>> ValidWeapons = AvailableWeapons;

    if (!Player) return ValidWeapons;

    TArray<TSubclassOf<ABaseWeapon>> PlayerCurrentWeapons = Player->GetOwnedWeaponClasses();

    for (const TSubclassOf<ABaseWeapon>& WeaponClass : PlayerCurrentWeapons)
    {
        ValidWeapons.Remove(WeaponClass);
    }

    return ValidWeapons;
}

void AMysteryBox::FinishSpin()
{
    // لە کاتی Fire Saleدا چانسی دەرچوونی Teddy Bear دەبێتە 0
    bool bShouldSpawnTeddy = !bIsFireSaleActive && (CurrentSpinCount >= MinSpinsBeforeTeddy) && (FMath::FRand() <= TeddyBearChance);

    if (bShouldSpawnTeddy)
    {
        HandleTeddyBear();
        return;
    }

    TArray<TSubclassOf<ABaseWeapon>> FilteredWeapons = GetFilteredWeaponsForPlayer(CurrentBuyer);

    if (FilteredWeapons.Num() == 0)
    {
        FilteredWeapons = AvailableWeapons;
    }

    if (FilteredWeapons.Num() == 0)
    {
        ResetBox();
        return;
    }

    int32 RandomIndex = FMath::RandRange(0, FilteredWeapons.Num() - 1);
    OfferedWeaponClass = FilteredWeapons[RandomIndex];

    MARK_PROPERTY_DIRTY_FROM_NAME(AMysteryBox, OfferedWeaponClass, this);

    BoxState = EMysteryBoxState::WeaponOffered;
    MARK_PROPERTY_DIRTY_FROM_NAME(AMysteryBox, BoxState, this);
    UpdateVisuals();

    FlushNetDormancy();
    ForceNetUpdate();

    GetWorldTimerManager().SetTimer(TimerHandle_OfferTimeout, this, &AMysteryBox::ResetBox, OfferDuration, false);
}

void AMysteryBox::HandleTeddyBear()
{
    if (CurrentBuyer)
    {
        if (AHamaPlayerState* PS = CurrentBuyer->GetPlayerState<AHamaPlayerState>())
        {
            PS->AddPoints(GetCurrentPrice());
        }
    }

    CurrentSpinCount = 0;
    OfferedWeaponClass = nullptr;

    BoxState = EMysteryBoxState::TeddyBear;

    MARK_PROPERTY_DIRTY_FROM_NAME(AMysteryBox, BoxState, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AMysteryBox, OfferedWeaponClass, this);

    UpdateVisuals();

    FlushNetDormancy();
    ForceNetUpdate();

    GetWorldTimerManager().SetTimer(TimerHandle_TeddyBear, this, &AMysteryBox::RelocateBox, 4.0f, false);
}

void AMysteryBox::RelocateBox()
{
    if (!HasAuthority()) return;

    UZombieDirectorSubsystem* Director = GetWorld()->GetSubsystem<UZombieDirectorSubsystem>();
    AMysteryBoxSpawnPoint* NewPoint = Director ? Director->GetRandomFreeMysteryBoxPoint(CurrentSpawnPoint) : nullptr;

    if (NewPoint)
    {
        if (CurrentSpawnPoint)
        {
            CurrentSpawnPoint->SetOccupied(false);
        }

        CurrentSpawnPoint = NewPoint;
        CurrentSpawnPoint->SetOccupied(true);
        SetActorTransform(CurrentSpawnPoint->GetActorTransform());
    }

    ResetBox();
}

void AMysteryBox::ResetBox()
{
    BoxState = EMysteryBoxState::Cooldown;
    OfferedWeaponClass = nullptr;
    CurrentBuyer = nullptr;

    MARK_PROPERTY_DIRTY_FROM_NAME(AMysteryBox, BoxState, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AMysteryBox, OfferedWeaponClass, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AMysteryBox, CurrentBuyer, this);

    UpdateVisuals();

    FlushNetDormancy();
    ForceNetUpdate();

    GetWorldTimerManager().SetTimer(TimerHandle_ResetToIdle, this, &AMysteryBox::ResetToIdle, 1.5f, false);
}

void AMysteryBox::ResetToIdle()
{
    if (bPendingFireSaleDestroy)
    {
        if (CurrentSpawnPoint)
        {
            CurrentSpawnPoint->SetOccupied(false);
        }

        Destroy();
        return;
    }

    BoxState = EMysteryBoxState::Idle;
    MARK_PROPERTY_DIRTY_FROM_NAME(AMysteryBox, BoxState, this);
    UpdateVisuals();
}

void AMysteryBox::SetFireSaleActive(bool bActive)
{
    if (!HasAuthority()) return;

    bIsFireSaleActive = bActive;
    MARK_PROPERTY_DIRTY_FROM_NAME(AMysteryBox, bIsFireSaleActive, this);

    FlushNetDormancy();
    ForceNetUpdate();
}

void AMysteryBox::HandleFireSaleEnd()
{
    if (BoxState == EMysteryBoxState::Idle || BoxState == EMysteryBoxState::Cooldown)
    {
        if (CurrentSpawnPoint)
        {
            CurrentSpawnPoint->SetOccupied(false);
        }

        Destroy();
    }
    else
    {
        bPendingFireSaleDestroy = true;
    }
}

void AMysteryBox::OnRep_IsFireSaleActive()
{
    // لێدان یان گۆڕینی UI لەسەر کلاینتەکان لە کاتی دەستپێکردن/کۆتایی Fire Sale
}

void AMysteryBox::OnRep_BoxState()
{
    UpdateVisuals();
}

void AMysteryBox::OnRep_OfferedWeaponClass()
{
    UpdateVisuals();
}

void AMysteryBox::UpdateVisuals()
{
    if (BoxState == EMysteryBoxState::TeddyBear)
    {
        if (TeddyBearMesh)
        {
            OfferedWeaponMesh->SetStaticMesh(TeddyBearMesh);
            OfferedWeaponMesh->SetVisibility(true);
        }
        return;
    }

    if (BoxState == EMysteryBoxState::WeaponOffered && OfferedWeaponClass)
    {
        if (const ABaseWeapon* DefaultWeapon = OfferedWeaponClass->GetDefaultObject<ABaseWeapon>())
        {
            if (const UStaticMeshComponent* MeshComp = DefaultWeapon->FindComponentByClass<UStaticMeshComponent>())
            {
                if (UStaticMesh* Mesh = MeshComp->GetStaticMesh())
                {
                    OfferedWeaponMesh->SetStaticMesh(Mesh);
                    OfferedWeaponMesh->SetVisibility(true);
                    return;
                }
            }
        }
    }

    OfferedWeaponMesh->SetVisibility(false);
}