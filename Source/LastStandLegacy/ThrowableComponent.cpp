#include "ThrowableComponent.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Hama.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Grenade.h"
#include "MonkeyBomb.h"
#include "LastStandLegacyGameState.h"
#include "Engine/OverlapResult.h"

DEFINE_LOG_CATEGORY_STATIC(LogThrowableSystem, Log, All);

UThrowableComponent::UThrowableComponent()
{
#if UE_BUILD_SHIPPING
    PrimaryComponentTick.bCanEverTick = false;
    PrimaryComponentTick.bStartWithTickEnabled = false;
#else
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
#endif

    SetIsReplicatedByDefault(true);

    MaxGrenadeCookTime = 3.5f;
    GrenadeThrowCooldown = 1.5f;
    MonkeyThrowCooldown = 2.0f;

    ChargeState = EThrowChargeState::Idle;
    bIsMonkeyThrow = false;
    bIsThrowingLocal = false;
    LastThrowTime = -100.0f;
    ServerCookStartTime = 0.0f;

    MaxGrenadeCount = 5;
    CurrentGrenadeCount = 3;

    MaxMonkeyCount = 3;
    CurrentMonkeyCount = 0;

    ThrowableHandSocketName = TEXT("GrenadeHandSocket");
}

void UThrowableComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if !UE_BUILD_SHIPPING
    //Debug_RenderNetworkDesync();
#endif
}

void UThrowableComponent::Debug_RenderNetworkDesync()
{
#if !UE_BUILD_SHIPPING
    if (!GEngine || !GetOwner() || !GetWorld()) return;

    const bool bIsServer = GetOwner()->HasAuthority();
    const FString RoleName = bIsServer ? TEXT("SERVER") : TEXT("CLIENT");
    const float CurrentTime = GetWorld()->GetTimeSeconds();

    const float TimeSinceLastThrow = CurrentTime - LastThrowTime;
    const float ActiveCooldown = bIsMonkeyThrow ? MonkeyThrowCooldown : GrenadeThrowCooldown;
    const float RemainingCooldown = FMath::Max(0.0f, ActiveCooldown - TimeSinceLastThrow);
    const bool bIsOnCooldown = RemainingCooldown > 0.0f;

    TArray<FString> DesyncReasons;

    if (!bIsServer && bIsThrowingLocal && ChargeState == EThrowChargeState::Idle)
    {
        DesyncReasons.Add(TEXT("LocalThrow Active while State is IDLE"));
    }

    if (!bIsServer && (PendingGrenadeThrows > MaxGrenadeCount || PendingMonkeyThrows > MaxMonkeyCount))
    {
        DesyncReasons.Add(TEXT("Pending Throws Overflow/Stuck"));
    }

    if (CharacterOwner)
    {
        const bool bHandMeshVisible = bIsMonkeyThrow ?
            (CharacterOwner->MonkeyHandMesh && CharacterOwner->MonkeyHandMesh->IsVisible()) :
            (CharacterOwner->GrenadeHandMesh && CharacterOwner->GrenadeHandMesh->IsVisible());

        const bool bWeaponHidden = IsValid(CharacterOwner->CurrentWeapon) && CharacterOwner->CurrentWeapon->IsHidden();

        if (ChargeState == EThrowChargeState::Idle && (bHandMeshVisible || bWeaponHidden))
        {
            DesyncReasons.Add(TEXT("Visual Mismatch (Weapon hidden or Hand Mesh visible in IDLE)"));
        }
        else if (ChargeState == EThrowChargeState::Charging && (!bHandMeshVisible || !bWeaponHidden))
        {
            DesyncReasons.Add(TEXT("Visual Mismatch (Weapon visible or Hand Mesh hidden in CHARGING)"));
        }
    }

    if (!bIsServer)
    {
        const bool bLocalTimerActive = GetWorld()->GetTimerManager().IsTimerActive(TimerHandle_LocalCookExplosion);
        if (bLocalTimerActive && ChargeState != EThrowChargeState::Charging)
        {
            DesyncReasons.Add(TEXT("Local Cook Timer active outside CHARGING state"));
        }
    }

    const bool bDesyncDetected = DesyncReasons.Num() > 0;

    FColor DisplayColor = FColor::Green;
    if (bDesyncDetected)
    {
        DisplayColor = FColor::Red;
    }
    else if (bIsOnCooldown)
    {
        DisplayColor = FColor::Yellow;
    }
    else if (!bIsServer)
    {
        DisplayColor = FColor::Cyan;
    }

    FString DesyncDetailsStr = TEXT("");
    if (bDesyncDetected)
    {
        DesyncDetailsStr = FString::Printf(TEXT(" | [DESYNC: %s]"), *FString::Join(DesyncReasons, TEXT(", ")));
    }

    FString DebugMsg = FString::Printf(
        TEXT("[%s] Grenades: %d (Pending: %d) | Monkeys: %d (Pending: %d) | Cooldown: %.2fs | State: %d | LocalThrow: %s%s"),
        *RoleName,
        CurrentGrenadeCount, PendingGrenadeThrows,
        CurrentMonkeyCount, PendingMonkeyThrows,
        RemainingCooldown,
        static_cast<int32>(ChargeState),
        bIsThrowingLocal ? TEXT("TRUE") : TEXT("FALSE"),
        *DesyncDetailsStr
    );

    int32 DebugKey = GetOwner()->GetUniqueID() + (bIsServer ? 1000 : 2000);
    GEngine->AddOnScreenDebugMessage(DebugKey, 0.0f, DisplayColor, DebugMsg);
#endif
}

void UThrowableComponent::BeginPlay()
{
    Super::BeginPlay();

    CharacterOwner = Cast<AHama>(GetOwner());

    if (GrenadeThrowMontage)
    {
        GrenadeThrowCooldown = GrenadeThrowMontage->GetPlayLength();
    }

    if (MonkeyThrowMontage)
    {
        MonkeyThrowCooldown = MonkeyThrowMontage->GetPlayLength();
    }

    if (CharacterOwner && CharacterOwner->IsLocallyControlled())
    {
        OnThrowableCountChanged.ExecuteIfBound(CurrentMonkeyCount, CurrentGrenadeCount);
    }
}

void UThrowableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams RepParams;
    RepParams.bIsPushBased = true;

    RepParams.Condition = COND_SkipOwner;
    DOREPLIFETIME_WITH_PARAMS_FAST(UThrowableComponent, ChargeState, RepParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(UThrowableComponent, bIsMonkeyThrow, RepParams);

    RepParams.Condition = COND_OwnerOnly;
    DOREPLIFETIME_WITH_PARAMS_FAST(UThrowableComponent, CurrentGrenadeCount, RepParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(UThrowableComponent, CurrentMonkeyCount, RepParams);
}

void UThrowableComponent::StartGrenadeCharge() { Internal_StartCharge(false); }
void UThrowableComponent::StartMonkeyCharge() { Internal_StartCharge(true); }

void UThrowableComponent::Internal_StartCharge(bool bIsMonkey)
{
    if (!CharacterOwner || !GetWorld())
    {
        UE_LOG(LogThrowableSystem, Warning, TEXT("[StartCharge FAILED] CharacterOwner or World is NULL!"));
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("StartCharge Fail: Invalid Owner/World"));
        return;
    }

    const uint8 ServerCount = bIsMonkey ? CurrentMonkeyCount : CurrentGrenadeCount;
    const uint8 PendingCount = bIsMonkey ? PendingMonkeyThrows : PendingGrenadeThrows;
    const int32 EffectiveCount = FMath::Max(0, static_cast<int32>(ServerCount) - PendingCount);

    const TSubclassOf<AActor> TargetClass = bIsMonkey ? MonkeyClass : GrenadeClass;

    if (EffectiveCount <= 0)
    {
        return;
    }

    if (!TargetClass)
    {
        return;
    }

    if (CharacterOwner->IsDrinkingPerk() || CharacterOwner->IsMeleeing() ||
        CharacterOwner->IsDiving() || CharacterOwner->IsDowned() || CharacterOwner->bIsDead)
    {
        return;
    }

    const float ActiveCooldown = bIsMonkey ? MonkeyThrowCooldown : GrenadeThrowCooldown;
    const float CurrentTime = GetWorld()->GetTimeSeconds();

    if (bIsThrowingLocal)
    {
        return;
    }

    if (ChargeState != EThrowChargeState::Idle)
    {
        return;
    }

    if (CurrentTime - LastThrowTime < ActiveCooldown)
    {
        const float Remaining = ActiveCooldown - (CurrentTime - LastThrowTime);
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, FString::Printf(TEXT("StartCharge Fail: Cooldown (%.1fs)"), Remaining));
        return;
    }

    bIsThrowingLocal = true;
    bIsMonkeyThrow = bIsMonkey;

    if (!GetOwner()->HasAuthority())
    {
        if (!bIsMonkey)
        {
            GetWorld()->GetTimerManager().SetTimer(
                TimerHandle_LocalCookExplosion,
                this,
                &UThrowableComponent::Local_OnGrenadeCookExpired,
                MaxGrenadeCookTime,
                false
            );
        }

        ChargeState = EThrowChargeState::Charging;
        HandleChargeStateChanged();
        Server_StartCharge(bIsMonkey);
    }
    else
    {
        ExecuteStartCharge_Server(bIsMonkey);
    }
}

void UThrowableComponent::ExecuteStartCharge_Server(bool bIsMonkey)
{
    if (!CharacterOwner) return;

    if (CharacterOwner->IsDrinkingPerk() || CharacterOwner->IsMeleeing() ||
        CharacterOwner->IsDiving() || CharacterOwner->IsDowned() || CharacterOwner->bIsDead)
    {
        ResetThrowState_Server();
        Client_RejectThrow();
        return;
    }

    const uint8 CurrentCount = bIsMonkey ? CurrentMonkeyCount : CurrentGrenadeCount;
    if (CurrentCount <= 0 || ChargeState != EThrowChargeState::Idle)
    {
        ResetThrowState_Server();
        Client_RejectThrow();
        return;
    }

    bIsMonkeyThrow = bIsMonkey;
    ChargeState = EThrowChargeState::Charging;
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, ChargeState, this);

    ServerCookStartTime = GetWorld()->GetTimeSeconds();

    if (!bIsMonkey && GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            TimerHandle_CookExplosion,
            this,
            &UThrowableComponent::Server_OnGrenadeCookExpired,
            MaxGrenadeCookTime,
            false
        );
    }

    HandleChargeStateChanged();
}

void UThrowableComponent::Server_StartCharge_Implementation(bool bIsMonkey)
{
    if (!CharacterOwner) return;

    if (ChargeState != EThrowChargeState::Idle)
    {
        Client_RejectThrow();
        return;
    }

    if (CharacterOwner->IsDrinkingPerk() || CharacterOwner->IsMeleeing() ||
        CharacterOwner->IsDiving() || CharacterOwner->IsDowned() || CharacterOwner->bIsDead)
    {
        Client_RejectThrow();
        return;
    }

    const int32 TargetCount = bIsMonkey ? CurrentMonkeyCount : CurrentGrenadeCount;
    const TSubclassOf<AActor> TargetClass = bIsMonkey ? MonkeyClass : GrenadeClass;
    const float ActiveCooldown = bIsMonkey ? MonkeyThrowCooldown : GrenadeThrowCooldown;
    const float CurrentTime = GetWorld()->GetTimeSeconds();

    float PlayerPingSeconds = 0.03f;
    if (CharacterOwner->GetPlayerState())
    {
        PlayerPingSeconds = FMath::Max(0.02f, CharacterOwner->GetPlayerState()->GetPingInMilliseconds() * 0.001f);
    }
    const float DynamicMargin = FMath::Clamp(PlayerPingSeconds + 0.02f, 0.03f, 0.15f);
    const bool bCooldownPassed = (CurrentTime - LastThrowTime) >= (ActiveCooldown - DynamicMargin);

    if (!TargetClass || TargetCount <= 0 || !bCooldownPassed || ChargeState != EThrowChargeState::Idle)
    {
        ResetThrowState_Server();
        Client_RejectThrow();
        return;
    }

    ExecuteStartCharge_Server(bIsMonkey);
}

void UThrowableComponent::ReleaseGrenadeThrow() { Internal_ReleaseThrow(false); }
void UThrowableComponent::ReleaseMonkeyThrow() { Internal_ReleaseThrow(true); }

void UThrowableComponent::Internal_ReleaseThrow(bool bIsMonkey)
{
    if (!CharacterOwner)
    {
        return;
    }

    if (bIsMonkey != bIsMonkeyThrow)
    {
        return;
    }

    if (ChargeState != EThrowChargeState::Charging)
    {
        return;
    }

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(TimerHandle_LocalCookExplosion);
    }

    FVector ClientViewLoc;
    FRotator ClientViewRot;

    if (APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController()))
    {
        PC->GetPlayerViewPoint(ClientViewLoc, ClientViewRot);
    }
    else
    {
        ClientViewLoc = CharacterOwner->GetPawnViewLocation();
        ClientViewRot = CharacterOwner->GetBaseAimRotation();
    }

    FVector ClientAimDir = ClientViewRot.Vector();

  
    if (!GetOwner()->HasAuthority())
    {
        if (bIsMonkey) { PendingMonkeyThrows++; }
        else { PendingGrenadeThrows++; }

        ChargeState = EThrowChargeState::Released;
        HandleChargeStateChanged();

        Server_ExecuteThrow(ClientViewLoc, ClientAimDir, bIsMonkey);
        LastThrowTime = GetWorld()->GetTimeSeconds();
    }
    else
    {
        Server_ExecuteThrow(ClientViewLoc, ClientAimDir, bIsMonkey);
    }
}

void UThrowableComponent::Server_ExecuteThrow_Implementation(FVector_NetQuantize ClientViewLoc, FVector_NetQuantizeNormal ClientAimDir, bool bIsMonkey)
{
    if (!CharacterOwner) return;

    if (CharacterOwner->IsDrinkingPerk() || CharacterOwner->IsMeleeing() ||
        CharacterOwner->IsDiving() || CharacterOwner->IsDowned() || CharacterOwner->bIsDead)
    {
        ResetThrowState_Server();
        Client_RejectThrow();
        return;
    }

    if (bIsMonkey != bIsMonkeyThrow)
    {
        ResetThrowState_Server();
        Client_RejectThrow();
        return;
    }

    const bool bActualIsMonkey = bIsMonkeyThrow;
    TSubclassOf<AActor> TargetClass = bActualIsMonkey ? MonkeyClass : GrenadeClass;
    uint8& TargetCount = bActualIsMonkey ? CurrentMonkeyCount : CurrentGrenadeCount;
    const float ActiveCooldown = bActualIsMonkey ? MonkeyThrowCooldown : GrenadeThrowCooldown;
    const float CurrentTime = GetWorld()->GetTimeSeconds();

    float PlayerPingSeconds = 0.03f;
    if (CharacterOwner->GetPlayerState())
    {
        PlayerPingSeconds = FMath::Max(0.02f, CharacterOwner->GetPlayerState()->GetPingInMilliseconds() * 0.001f);
    }
    const float DynamicMargin = FMath::Clamp(PlayerPingSeconds + 0.02f, 0.03f, 0.15f);
    const bool bIsCooldownActive = (CurrentTime - LastThrowTime) < (ActiveCooldown - DynamicMargin);
    const bool bValidState = (ChargeState == EThrowChargeState::Charging);

    if (!TargetClass)
    {
        ResetThrowState_Server();
        Client_RejectThrow();
        return;
    }

    if (TargetCount <= 0)
    {
        ResetThrowState_Server();
        Client_RejectThrow();
        return;
    }

    if (!bValidState)
    {
        ResetThrowState_Server();
        Client_RejectThrow();
        return;
    }

    if (bIsCooldownActive)
    {
        ResetThrowState_Server();
        Client_RejectThrow();
        return;
    }

    if (!bActualIsMonkey && GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(TimerHandle_CookExplosion);
    }

    LastThrowTime = CurrentTime;
    ChargeState = EThrowChargeState::Released;

    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, ChargeState, this);

    HandleChargeStateChanged();

    const FVector ServerEyeLoc = CharacterOwner->GetPawnViewLocation();

    FVector ValidatedViewLoc = ClientViewLoc;
    if (FVector::DistSquared(ClientViewLoc, ServerEyeLoc) > FMath::Square(200.0f))
    {
        ValidatedViewLoc = ServerEyeLoc;
    }

    FVector ServerAimDir = CharacterOwner->GetBaseAimRotation().Vector();
    FVector ValidatedAimDir = ClientAimDir.GetSafeNormal();

    if (ValidatedAimDir.IsNearlyZero() || FVector::DotProduct(ValidatedAimDir, ServerAimDir) < 0.7f)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("[Server ExecuteThrow] Invalid AimDir from Client! Using Server AimDir instead."));
        ValidatedAimDir = ServerAimDir;
    }

    FVector TargetPoint = TraceCrosshairTarget(ValidatedViewLoc, ValidatedAimDir);
    FVector StartLoc = ServerEyeLoc + (ValidatedAimDir * 40.0f);

    FVector FinalSpawnLoc;
    GetSafeSpawnLocation(ServerEyeLoc, StartLoc, FinalSpawnLoc);
    FVector TrueLaunchDir = (TargetPoint - FinalSpawnLoc).GetSafeNormal();

    FRotator SpawnRotation = TrueLaunchDir.Rotation();
    FTransform SpawnTransform(SpawnRotation, FinalSpawnLoc);

    float RemainingFuseTime = MaxGrenadeCookTime;
    if (!bActualIsMonkey)
    {
        float ElapsedCookTime = CurrentTime - ServerCookStartTime;
        RemainingFuseTime = FMath::Max(0.1f, MaxGrenadeCookTime - ElapsedCookTime);
    }

    AActor* SpawnedThrowable = GetWorld()->SpawnActorDeferred<AActor>(
        TargetClass, SpawnTransform, CharacterOwner, CharacterOwner, ESpawnActorCollisionHandlingMethod::AlwaysSpawn
    );

    if (SpawnedThrowable)
    {
        if (AGrenade* GrenadeActor = Cast<AGrenade>(SpawnedThrowable))
        {
            GrenadeActor->SetFuseDuration(RemainingFuseTime);
            UGameplayStatics::FinishSpawningActor(SpawnedThrowable, SpawnTransform);
            GrenadeActor->InitVelocity(TrueLaunchDir * GrenadeThrowImpulseStrength);
        }
        else if (AMonkeyBomb* MonkeyActor = Cast<AMonkeyBomb>(SpawnedThrowable))
        {
            UGameplayStatics::FinishSpawningActor(SpawnedThrowable, SpawnTransform);
            MonkeyActor->InitVelocity(TrueLaunchDir * MonkeyThrowImpulseStrength);
        }
        else
        {
            UGameplayStatics::FinishSpawningActor(SpawnedThrowable, SpawnTransform);
        }

        TargetCount--;
        if (bActualIsMonkey)
        {
            MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentMonkeyCount, this);

            if (CharacterOwner && CharacterOwner->IsLocallyControlled())
            {
                OnRep_MonkeyCount();
            }
        }
        else
        {
            MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentGrenadeCount, this);

            if (CharacterOwner && CharacterOwner->IsLocallyControlled())
            {
                OnRep_GrenadeCount();
            }
        }
    }
    else
    {
        ResetThrowState_Server();
        Client_RejectThrow();
        return;
    }

    const float ResetDelay = FMath::Max(0.2f, ActiveCooldown - 0.1f);
    GetWorld()->GetTimerManager().SetTimer(
        TimerHandle_ResetThrowState, this, &UThrowableComponent::ResetThrowState_Server, ResetDelay, false
    );
}

void UThrowableComponent::Server_OnGrenadeCookExpired()
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority()) return;

    ChargeState = EThrowChargeState::Idle;
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, ChargeState, this);
    HandleChargeStateChanged();

    const FVector ExpLocation = CharacterOwner->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
    const float ExplosionRadius = 400.0f;
    const float BaseDamage = 500.0f;

    UGameplayStatics::ApplyDamage(
        CharacterOwner,
        BaseDamage,
        CharacterOwner->GetController(),
        CharacterOwner,
        UDamageType::StaticClass()
    );

    ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>();

    TArray<AActor*> IgnoredActors;
    IgnoredActors.Reserve(GS ? GS->PlayerArray.Num() + 1 : 1);
    IgnoredActors.Add(CharacterOwner);

    if (GS)
    {
        for (APlayerState* PS : GS->PlayerArray)
        {
            if (!PS) continue;
            if (APawn* PlayerPawn = PS->GetPawn())
            {
                if (PlayerPawn != CharacterOwner)
                {
                    IgnoredActors.Add(PlayerPawn);
                }
            }
        }
    }

    UGameplayStatics::ApplyRadialDamage(
        this,
        BaseDamage,
        ExpLocation,
        ExplosionRadius,
        UDamageType::StaticClass(),
        IgnoredActors,
        CharacterOwner,
        CharacterOwner->GetController(),
        false,
        ECC_WorldStatic
    );

    CurrentGrenadeCount = FMath::Max(0, CurrentGrenadeCount - 1);
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentGrenadeCount, this);

    if (CharacterOwner->IsLocallyControlled())
    {
        OnRep_GrenadeCount();
    }

    Multicast_OnGrenadeCookExpiredFX();
    ResetThrowState_Server();
    Client_RejectThrow();
}

void UThrowableComponent::Multicast_OnGrenadeCookExpiredFX_Implementation()
{
    Local_OnGrenadeCookExpired();
}

void UThrowableComponent::Local_OnGrenadeCookExpired()
{
    bIsThrowingLocal = false;
    ChargeState = EThrowChargeState::Idle;

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(TimerHandle_CookExplosion);
    }

    HandleChargeStateChanged();
}

void UThrowableComponent::ResetThrowState_Server()
{
    ChargeState = EThrowChargeState::Idle;

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(TimerHandle_CookExplosion);
        GetWorld()->GetTimerManager().ClearTimer(TimerHandle_ResetThrowState);
    }

    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, ChargeState, this);
    HandleChargeStateChanged();
}

void UThrowableComponent::Client_RejectThrow_Implementation()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(TimerHandle_LocalCookExplosion);
        World->GetTimerManager().ClearTimer(TimerHandle_ResetThrowState);
    }

    PendingGrenadeThrows = 0;
    PendingMonkeyThrows = 0;

    bIsThrowingLocal = false;
    ChargeState = EThrowChargeState::Idle;
    HandleChargeStateChanged();
}

void UThrowableComponent::OnThrowMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        ResetThrowState_Server();
    }
    else if (CharacterOwner && CharacterOwner->IsLocallyControlled())
    {
        bIsThrowingLocal = false;
        ChargeState = EThrowChargeState::Idle;
        HandleChargeStateChanged();
    }
}

void UThrowableComponent::OnRep_ChargeState()
{
    if (CharacterOwner && CharacterOwner->IsLocallyControlled())
    {
        if (ChargeState == EThrowChargeState::Idle)
        {
            bIsThrowingLocal = false;
            if (UWorld* World = GetWorld())
            {
                World->GetTimerManager().ClearTimer(TimerHandle_LocalCookExplosion);
            }
        }
    }

    HandleChargeStateChanged();
}

void UThrowableComponent::HandleChargeStateChanged()
{
    if (ChargeState == EThrowChargeState::Idle)
    {
        bIsThrowingLocal = false;

        if (GetWorld())
        {
            GetWorld()->GetTimerManager().ClearTimer(TimerHandle_CookExplosion);
        }
    }

    if (!CharacterOwner || !CharacterOwner->GetMesh()) return;

    UAnimInstance* AnimInstance = CharacterOwner->GetMesh()->GetAnimInstance();
    UAnimMontage* MontageToPlay = bIsMonkeyThrow ? MonkeyThrowMontage : GrenadeThrowMontage;

    switch (ChargeState)
    {
    case EThrowChargeState::Charging:
    {
        PlayThrowMontageWithDelegate(MontageToPlay, HoldSectionName);
        SetWeaponHidden(true);
        ToggleHandThrowableVisibility(true);
        break;
    }
    case EThrowChargeState::Released:
    {
        PlayThrowMontageWithDelegate(MontageToPlay, ReleaseSectionName);
        ToggleHandThrowableVisibility(false);
        break;
    }
    case EThrowChargeState::Idle:
    default:
    {
        if (AnimInstance && MontageToPlay && AnimInstance->Montage_IsPlaying(MontageToPlay))
        {
            AnimInstance->Montage_Stop(0.2f, MontageToPlay);
        }

        SetWeaponHidden(false);
        ToggleHandThrowableVisibility(false);
        break;
    }
    }
}

void UThrowableComponent::PlayThrowMontageWithDelegate(UAnimMontage* MontageToPlay, FName SectionName)
{
    if (!CharacterOwner || !CharacterOwner->GetMesh() || !MontageToPlay) return;

    UAnimInstance* AnimInstance = CharacterOwner->GetMesh()->GetAnimInstance();
    if (!AnimInstance) return;

    const bool bIsPlaying = AnimInstance->Montage_IsPlaying(MontageToPlay);

    if (!bIsPlaying)
    {
        AnimInstance->Montage_Play(MontageToPlay, 1.0f);
    }

    if (SectionName != NAME_None)
    {
        AnimInstance->Montage_JumpToSection(SectionName, MontageToPlay);
    }

    FOnMontageEnded EndDelegate;
    EndDelegate.BindUObject(this, &UThrowableComponent::OnThrowMontageEnded);
    AnimInstance->Montage_SetEndDelegate(EndDelegate, MontageToPlay);
}

FVector UThrowableComponent::TraceCrosshairTarget(const FVector& ViewLoc, const FVector& AimDir) const
{
    if (!GetWorld() || !CharacterOwner)
    {
        return ViewLoc + (AimDir * 10000.0f);
    }

    const FVector TraceEnd = ViewLoc + (AimDir * 10000.0f);
    FHitResult Hit;

    FCollisionQueryParams TraceParams;
    TraceParams.AddIgnoredActor(CharacterOwner);

    if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLoc, TraceEnd, ECC_Visibility, TraceParams))
    {
        return Hit.ImpactPoint;
    }

    return TraceEnd;
}

void UThrowableComponent::GetSafeSpawnLocation(const FVector& StartLoc, const FVector& TargetLoc, FVector& OutSpawnLoc) const
{
    if (!GetWorld() || !CharacterOwner)
    {
        OutSpawnLoc = TargetLoc;
        return;
    }

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(CharacterOwner);

    if (GetWorld()->LineTraceSingleByChannel(HitResult, StartLoc, TargetLoc, ECC_WorldStatic, QueryParams))
    {
        OutSpawnLoc = HitResult.Location - (TargetLoc - StartLoc).GetSafeNormal() * 10.0f;
    }
    else
    {
        OutSpawnLoc = TargetLoc;
    }
}

void UThrowableComponent::ToggleHandThrowableVisibility(bool bVisible)
{
    if (IsRunningDedicatedServer() || !CharacterOwner) return;

    if (bIsMonkeyThrow)
    {
        if (CharacterOwner->MonkeyHandMesh)
        {
            CharacterOwner->MonkeyHandMesh->SetVisibility(bVisible);
        }
    }
    else
    {
        if (CharacterOwner->GrenadeHandMesh)
        {
            CharacterOwner->GrenadeHandMesh->SetVisibility(bVisible);
        }
    }
}

void UThrowableComponent::SetWeaponHidden(bool bHidden)
{
    if (IsValid(CharacterOwner) && IsValid(CharacterOwner->CurrentWeapon))
    {
        CharacterOwner->CurrentWeapon->SetActorHiddenInGame(bHidden);
    }
}

void UThrowableComponent::UnlockAndRefillMonkeyBomb(TSubclassOf<AActor> NewMonkeyClass, int32 Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (NewMonkeyClass) MonkeyClass = NewMonkeyClass;

    CurrentMonkeyCount = FMath::Clamp(Amount, 0, MaxMonkeyCount);
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentMonkeyCount, this);
    OnRep_MonkeyCount();
}

void UThrowableComponent::RefillMonkeyToMax()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !MonkeyClass) return;
    CurrentMonkeyCount = MaxMonkeyCount;
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentMonkeyCount, this);
    OnRep_MonkeyCount();
}

void UThrowableComponent::RefillGrenadesToMax()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    CurrentGrenadeCount = MaxGrenadeCount;
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentGrenadeCount, this);
    OnRep_GrenadeCount();
}

void UThrowableComponent::OnRep_GrenadeCount()
{
    PendingGrenadeThrows = FMath::Max(0, PendingGrenadeThrows - 1);
    OnThrowableCountChanged.ExecuteIfBound(CurrentMonkeyCount, CurrentGrenadeCount);
}

void UThrowableComponent::OnRep_MonkeyCount()
{
    PendingMonkeyThrows = FMath::Max(0, PendingMonkeyThrows - 1);
    OnThrowableCountChanged.ExecuteIfBound(CurrentMonkeyCount, CurrentGrenadeCount);
}

void UThrowableComponent::HandleOwnerDowned()
{
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor || !OwnerActor->HasAuthority()) return;

    if (ChargeState == EThrowChargeState::Charging)
    {
        UWorld* World = GetWorld();
        if (World && CharacterOwner)
        {
            const float CurrentTime = World->GetTimeSeconds();
            const bool bActualIsMonkey = bIsMonkeyThrow;
            TSubclassOf<AActor> TargetClass = bActualIsMonkey ? MonkeyClass : GrenadeClass;
            uint8& TargetCount = bActualIsMonkey ? CurrentMonkeyCount : CurrentGrenadeCount;

            if (TargetClass && TargetCount > 0)
            {
                if (!bActualIsMonkey)
                {
                    World->GetTimerManager().ClearTimer(TimerHandle_CookExplosion);
                }

                const float ElapsedCookTime = CurrentTime - ServerCookStartTime;
                const float RemainingFuseTime = bActualIsMonkey ? MaxGrenadeCookTime : FMath::Max(0.1f, MaxGrenadeCookTime - ElapsedCookTime);

                FVector DropLocation = CharacterOwner->GetActorLocation() + FVector(0.0f, 0.0f, 20.0f);
                FTransform DropTransform(CharacterOwner->GetActorRotation(), DropLocation);

                AActor* DroppedThrowable = World->SpawnActorDeferred<AActor>(
                    TargetClass, DropTransform, CharacterOwner, CharacterOwner, ESpawnActorCollisionHandlingMethod::AlwaysSpawn
                );

                if (DroppedThrowable)
                {
                    if (AGrenade* GrenadeActor = Cast<AGrenade>(DroppedThrowable))
                    {
                        GrenadeActor->SetFuseDuration(RemainingFuseTime);
                        UGameplayStatics::FinishSpawningActor(DroppedThrowable, DropTransform);
                        GrenadeActor->InitVelocity(FVector(FMath::RandRange(-80.0f, 80.0f), FMath::RandRange(-80.0f, 80.0f), 50.0f));
                    }
                    else if (AMonkeyBomb* MonkeyActor = Cast<AMonkeyBomb>(DroppedThrowable))
                    {
                        UGameplayStatics::FinishSpawningActor(DroppedThrowable, DropTransform);
                        MonkeyActor->InitVelocity(FVector(0.0f, 0.0f, 30.0f));
                    }
                    else
                    {
                        UGameplayStatics::FinishSpawningActor(DroppedThrowable, DropTransform);
                    }

                    TargetCount--;
                    if (bActualIsMonkey)
                    {
                        MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentMonkeyCount, this);
                    }
                    else
                    {
                        MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentGrenadeCount, this);
                    }
                }
            }
        }
    }

    ResetThrowState_Server();

    if (CharacterOwner)
    {
        if (CharacterOwner->IsLocallyControlled())
        {
            bIsThrowingLocal = false;
            if (UWorld* World = GetWorld())
            {
                World->GetTimerManager().ClearTimer(TimerHandle_LocalCookExplosion);
            }
        }
        else
        {
            Client_RejectThrow();
        }
    }
}