#include "MysteryBox.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Hama.h"
#include "BaseWeapon.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TimelineComponent.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "HamaPlayerState.h"
#include "MysteryBoxSpawnPoint.h"
#include "ZombieDirectorSubsystem.h"

AMysteryBox::AMysteryBox()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    SetReplicatingMovement(false);
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

    RiseTimelineComponent = CreateDefaultSubobject<UTimelineComponent>(TEXT("RiseTimelineComponent"));

    LightBeamVFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("LightBeamVFX"));
    LightBeamVFX->SetupAttachment(BoxBaseMesh);
    LightBeamVFX->SetAutoActivate(false);
}

void AMysteryBox::BeginPlay()
{
    Super::BeginPlay();

    if (OfferedWeaponMesh)
    {
        InitialOfferedMeshRelativeLocation = OfferedWeaponMesh->GetRelativeTransform().GetLocation();
    }

    if (RiseCurve && RiseTimelineComponent)
    {
        FOnTimelineFloat ProgressUpdate;
        ProgressUpdate.BindUFunction(this, FName("HandleRiseTimelineProgress"));
        RiseTimelineComponent->AddInterpFloat(RiseCurve, ProgressUpdate);
    }

    CacheWeaponMeshes();

    if (HasAuthority())
    {
        if (UZombieDirectorSubsystem* Director = GetWorld()->GetSubsystem<UZombieDirectorSubsystem>())
        {
            Director->RegisterMysteryBox(this);
        }

        GetWorldTimerManager().SetTimer(TimerHandle_InitialSetup, this, &AMysteryBox::TryInitialSpawnSetup, 0.2f, true);
    }
}

void AMysteryBox::TryInitialSpawnSetup()
{
    if (CurrentSpawnPoint != nullptr)
    {
        GetWorldTimerManager().ClearTimer(TimerHandle_InitialSetup);
        return;
    }

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

            GetWorldTimerManager().ClearTimer(TimerHandle_InitialSetup);
        }
    }
}

void AMysteryBox::CacheWeaponMeshes()
{
    CachedWeaponMeshes.Reset();
    for (const TSubclassOf<ABaseWeapon>& WeaponClass : AvailableWeapons)
    {
        if (WeaponClass)
        {
            if (const ABaseWeapon* DefaultWeapon = WeaponClass->GetDefaultObject<ABaseWeapon>())
            {
                if (const UStaticMeshComponent* MeshComp = DefaultWeapon->FindComponentByClass<UStaticMeshComponent>())
                {
                    if (UStaticMesh* Mesh = MeshComp->GetStaticMesh())
                    {
                        CachedWeaponMeshes.Add(Mesh);
                    }
                }
            }
        }
    }
}

void AMysteryBox::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(TimerHandle_InitialSetup);
    GetWorldTimerManager().ClearTimer(TimerHandle_VisualSpinCycle);
    GetWorldTimerManager().ClearAllTimersForObject(this);

    if (HasAuthority())
    {
        if (UZombieDirectorSubsystem* Director = GetWorld()->GetSubsystem<UZombieDirectorSubsystem>())
        {
            Director->UnregisterMysteryBox(this);
        }
    }

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
    if (!IsValid(InteractingPlayer)) return false;

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
        return IsValid(CurrentBuyer) && InteractingPlayer == CurrentBuyer;
    }

    return false;
}

void AMysteryBox::Interact(AHama* Player)
{
    if (!HasAuthority() || !IsValid(Player) || !CanInteract(Player)) return;

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
    HandleBoxStateChanged();

    FlushNetDormancy();
    ForceNetUpdate();

    GetWorldTimerManager().SetTimer(TimerHandle_Spin, this, &AMysteryBox::FinishSpin, SpinDuration, false);
}

TArray<TSubclassOf<ABaseWeapon>> AMysteryBox::GetFilteredWeaponsForPlayer(AHama* Player) const
{
    TArray<TSubclassOf<ABaseWeapon>> ValidWeapons = AvailableWeapons;

    if (!IsValid(Player)) return ValidWeapons;

    TArray<TSubclassOf<ABaseWeapon>> PlayerCurrentWeapons = Player->GetOwnedWeaponClasses();

    for (const TSubclassOf<ABaseWeapon>& WeaponClass : PlayerCurrentWeapons)
    {
        ValidWeapons.Remove(WeaponClass);
    }

    return ValidWeapons;
}

void AMysteryBox::FinishSpin()
{
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
    HandleBoxStateChanged();

    FlushNetDormancy();
    ForceNetUpdate();

    GetWorldTimerManager().SetTimer(TimerHandle_OfferTimeout, this, &AMysteryBox::ResetBox, OfferDuration, false);
}

void AMysteryBox::HandleTeddyBear()
{
    if (IsValid(CurrentBuyer))
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

    HandleBoxStateChanged();

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

        FlushNetDormancy();
        ForceNetUpdate();
    }

    ResetBox();
}

void AMysteryBox::AssignSpawnPoint(AMysteryBoxSpawnPoint* NewSpawnPoint)
{
    CurrentSpawnPoint = NewSpawnPoint;

    if (CurrentSpawnPoint)
    {
        SetActorTransform(CurrentSpawnPoint->GetActorTransform());
    }

    if (HasAuthority())
    {
        FlushNetDormancy();
        ForceNetUpdate();
    }
}

void AMysteryBox::ResetBox()
{
    BoxState = EMysteryBoxState::Cooldown;
    OfferedWeaponClass = nullptr;
    CurrentBuyer = nullptr;

    MARK_PROPERTY_DIRTY_FROM_NAME(AMysteryBox, BoxState, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AMysteryBox, OfferedWeaponClass, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AMysteryBox, CurrentBuyer, this);

    HandleBoxStateChanged();

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
    HandleBoxStateChanged();
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
}

void AMysteryBox::HandleBoxStateChanged()
{
    if (GetNetMode() == NM_DedicatedServer) return;

    UpdateVisuals();

    switch (BoxState)
    {
    case EMysteryBoxState::Spinning:
    {
        if (OfferedWeaponMesh) OfferedWeaponMesh->SetVisibility(true);
        if (LightBeamVFX) LightBeamVFX->Activate();

        if (SpinSound) UGameplayStatics::PlaySoundAtLocation(this, SpinSound, GetActorLocation());

        if (RiseTimelineComponent)
        {
            RiseTimelineComponent->PlayFromStart();
        }

        GetWorldTimerManager().SetTimer(TimerHandle_VisualSpinCycle, this, &AMysteryBox::CycleRandomWeaponMesh, 0.07f, true);
        break;
    }

    case EMysteryBoxState::WeaponOffered:
    {
        GetWorldTimerManager().ClearTimer(TimerHandle_VisualSpinCycle);

        if (RiseCurve && RiseTimelineComponent)
        {
            HandleRiseTimelineProgress(1.0f);
        }
        break;
    }

    case EMysteryBoxState::TeddyBear:
    {
        GetWorldTimerManager().ClearTimer(TimerHandle_VisualSpinCycle);

        if (TeddyBearSound)
        {
            UGameplayStatics::PlaySoundAtLocation(this, TeddyBearSound, GetActorLocation());
        }

        if (RiseTimelineComponent)
        {
            RiseTimelineComponent->PlayFromStart();
        }
        break;
    }

    case EMysteryBoxState::Cooldown:
    case EMysteryBoxState::Idle:
    {
        GetWorldTimerManager().ClearTimer(TimerHandle_VisualSpinCycle);
        if (RiseTimelineComponent) RiseTimelineComponent->Stop();
        if (LightBeamVFX) LightBeamVFX->Deactivate();

        if (OfferedWeaponMesh)
        {
            OfferedWeaponMesh->SetRelativeLocation(InitialOfferedMeshRelativeLocation);
            OfferedWeaponMesh->SetVisibility(false);
        }
        break;
    }
    }
}

void AMysteryBox::OnRep_BoxState()
{
    HandleBoxStateChanged();
}

void AMysteryBox::OnRep_OfferedWeaponClass()
{
    UpdateVisuals();
}

void AMysteryBox::CycleRandomWeaponMesh()
{
    if (CachedWeaponMeshes.Num() == 0 || !OfferedWeaponMesh) return;

    int32 RandomIndex = FMath::RandRange(0, CachedWeaponMeshes.Num() - 1);
    if (UStaticMesh* Mesh = CachedWeaponMeshes[RandomIndex])
    {
        OfferedWeaponMesh->SetStaticMesh(Mesh);
    }
}

void AMysteryBox::HandleRiseTimelineProgress(float Value)
{
    if (OfferedWeaponMesh)
    {
        FVector NewLoc = InitialOfferedMeshRelativeLocation;
        NewLoc.Z += (Value * MaxRiseHeight);
        OfferedWeaponMesh->SetRelativeLocation(NewLoc);
    }
}

void AMysteryBox::UpdateVisuals()
{
    if (BoxState == EMysteryBoxState::TeddyBear)
    {
        if (TeddyBearMesh && OfferedWeaponMesh)
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

    if (BoxState == EMysteryBoxState::Idle || BoxState == EMysteryBoxState::Cooldown)
    {
        if (OfferedWeaponMesh) OfferedWeaponMesh->SetVisibility(false);
    }
}