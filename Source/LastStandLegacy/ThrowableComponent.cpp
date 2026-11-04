#include "ThrowableComponent.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Hama.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Grenade.h"
#include "MonkeyBomb.h"
#include "LastStandLegacyGameState.h"
#include "ThrowablePoolSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogThrowableSystem, Log, All);

UThrowableComponent::UThrowableComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    PrimaryComponentTick.bStartWithTickEnabled = false;

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

void UThrowableComponent::BeginPlay()
{
    Super::BeginPlay();

    CharacterOwner = Cast<AHama>(GetOwner());

    if (GrenadeThrowMontage)
    {
        const int32 SectionIndex = GrenadeThrowMontage->GetSectionIndex(ReleaseSectionName);
        GrenadeThrowCooldown = (SectionIndex != INDEX_NONE) ? GrenadeThrowMontage->GetSectionLength(SectionIndex) : 0.6f;
    }

    if (MonkeyThrowMontage)
    {
        const int32 SectionIndex = MonkeyThrowMontage->GetSectionIndex(ReleaseSectionName);
        MonkeyThrowCooldown = (SectionIndex != INDEX_NONE) ? MonkeyThrowMontage->GetSectionLength(SectionIndex) : 0.8f;
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

    RepParams.Condition = COND_None;
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
    if (!CharacterOwner || !GetWorld()) return;

    const uint8 ServerCount = bIsMonkey ? CurrentMonkeyCount : CurrentGrenadeCount;
    const uint8 PendingCount = bIsMonkey ? PendingMonkeyThrows : PendingGrenadeThrows;
    const int32 EffectiveCount = FMath::Max(0, static_cast<int32>(ServerCount) - PendingCount);

    const TSubclassOf<AActor> TargetClass = bIsMonkey ? MonkeyClass : GrenadeClass;

    if (EffectiveCount <= 0 || !TargetClass || bIsThrowingLocal || ChargeState != EThrowChargeState::Idle)
    {
        return;
    }

    const float ActiveCooldown = bIsMonkey ? MonkeyThrowCooldown : GrenadeThrowCooldown;
    const float CurrentTime = GetWorld()->GetTimeSeconds();

    if (CurrentTime - LastThrowTime < ActiveCooldown)
    {
        return;
    }

    if (CharacterOwner->CurrentWeapon)
    {
        if (CharacterOwner->CurrentWeapon->IsReloading()) CharacterOwner->CurrentWeapon->CancelReload();
        if (CharacterOwner->bIsFireButtonHold) CharacterOwner->CurrentWeapon->StopFire();
    }

    bIsThrowingLocal = true;
    bIsMonkeyThrow = bIsMonkey;

    if (!GetOwner()->HasAuthority())
    {
        if (!bIsMonkey)
        {
            const float LocalCookTimerBuffer = MaxGrenadeCookTime + 0.2f;
            GetWorld()->GetTimerManager().SetTimer(
                TimerHandle_LocalCookExplosion,
                this,
                &UThrowableComponent::Local_OnGrenadeCookExpired,
                LocalCookTimerBuffer,
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

    if (CharacterOwner->IsSprinting())
    {
        CharacterOwner->StopSprint();
    }

    if (bIsMonkeyThrow != bIsMonkey)
    {
        bIsMonkeyThrow = bIsMonkey;
        MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, bIsMonkeyThrow, this);
    }

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

    if (ChargeState != EThrowChargeState::Idle ||
        CharacterOwner->IsDrinkingPerk() || CharacterOwner->IsMeleeing() ||
        CharacterOwner->IsDiving() || CharacterOwner->IsDowned() || CharacterOwner->bIsDead)
    {
        Client_RejectThrow();
        return;
    }

    if (CharacterOwner->CurrentWeapon)
    {
        if (CharacterOwner->CurrentWeapon->IsReloading()) CharacterOwner->CurrentWeapon->CancelReload();
        if (CharacterOwner->bIsFireButtonHold) CharacterOwner->CurrentWeapon->StopFire();
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
    if (!CharacterOwner || bIsMonkey != bIsMonkeyThrow || ChargeState != EThrowChargeState::Charging)
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
        CharacterOwner->IsDiving() || CharacterOwner->IsDowned() || CharacterOwner->bIsDead ||
        bIsMonkey != bIsMonkeyThrow)
    {
        ResetThrowState_Server();
        Client_RejectThrow();
        return;
    }

    const bool bActualIsMonkey = bIsMonkeyThrow;
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

    if (TargetCount <= 0 || !bValidState || bIsCooldownActive)
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

    const FVector ValidatedViewLoc = ClientViewLoc;
    const FVector ValidatedAimDir = ClientAimDir.GetSafeNormal();

    const FVector TargetPoint = TraceCrosshairTarget(ValidatedViewLoc, ValidatedAimDir);
    const FVector StartLoc = ValidatedViewLoc + (ValidatedAimDir * 80.f);

    FVector FinalSpawnLoc;
    GetSafeSpawnLocation(ValidatedViewLoc, StartLoc, FinalSpawnLoc);

    const FVector ToTarget = TargetPoint - FinalSpawnLoc;
    FVector TrueLaunchDir = ValidatedAimDir;

    if (ToTarget.SizeSquared() > FMath::Square(25.0f))
    {
        TrueLaunchDir = ToTarget.GetSafeNormal();
    }

    TrueLaunchDir = (TrueLaunchDir + FVector(0.0f, 0.0f, 0.05f)).GetSafeNormal();

    FVector InheritedVelocity = CharacterOwner->GetVelocity();
    InheritedVelocity.Z = 0.0f;

    const FVector GrenadeLaunchVelocity = (TrueLaunchDir * GrenadeThrowImpulseStrength) + InheritedVelocity;
    const FVector MonkeyLaunchVelocity = (TrueLaunchDir * MonkeyThrowImpulseStrength) + InheritedVelocity;

    const FRotator SpawnRotation = TrueLaunchDir.Rotation();
    const FTransform SpawnTransform(SpawnRotation, FinalSpawnLoc);

    float RemainingFuseTime = MaxGrenadeCookTime;
    if (!bActualIsMonkey)
    {
        const float ElapsedCookTime = CurrentTime - ServerCookStartTime;
        RemainingFuseTime = FMath::Max(0.1f, MaxGrenadeCookTime - ElapsedCookTime);
    }

    UThrowablePoolSubsystem* Pool = GetWorld()->GetSubsystem<UThrowablePoolSubsystem>();
    if (!Pool)
    {
        ResetThrowState_Server();
        Client_RejectThrow();
        return;
    }

    if (!bActualIsMonkey)
    {
        AGrenade* GrenadeActor = Pool->AcquireGrenade(GrenadeClass, SpawnTransform);
        if (GrenadeActor)
        {
            GrenadeActor->SetOwner(CharacterOwner);
            GrenadeActor->SetInstigator(CharacterOwner);

            if (UPrimitiveComponent* GrenadeRoot = Cast<UPrimitiveComponent>(GrenadeActor->GetRootComponent()))
            {
                GrenadeRoot->IgnoreActorWhenMoving(CharacterOwner, true);
                if (CharacterOwner->CurrentWeapon)
                {
                    GrenadeRoot->IgnoreActorWhenMoving(CharacterOwner->CurrentWeapon, true);
                }
            }

            GrenadeActor->ActivateFromPool(
                SpawnTransform,
                GrenadeLaunchVelocity,
                RemainingFuseTime,
                CharacterOwner->GetController()
            );

            TargetCount--;
            MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentGrenadeCount, this);
            if (CharacterOwner->IsLocallyControlled())
            {
                OnRep_GrenadeCount();
            }
        }
        else
        {
            ResetThrowState_Server();
            Client_RejectThrow();
            return;
        }
    }
    else
    {
        AMonkeyBomb* MonkeyActor = Pool->AcquireMonkey(MonkeyClass, SpawnTransform);
        if (MonkeyActor)
        {
            MonkeyActor->SetOwner(CharacterOwner);
            MonkeyActor->SetInstigator(CharacterOwner);

            if (UPrimitiveComponent* MonkeyRoot = Cast<UPrimitiveComponent>(MonkeyActor->GetRootComponent()))
            {
                MonkeyRoot->IgnoreActorWhenMoving(CharacterOwner, true);
                if (CharacterOwner->CurrentWeapon)
                {
                    MonkeyRoot->IgnoreActorWhenMoving(CharacterOwner->CurrentWeapon, true);
                }
            }

            MonkeyActor->ActivateFromPool(
                SpawnTransform,
                MonkeyLaunchVelocity,
                8.0f,
                CharacterOwner->GetController()
            );

            TargetCount--;
            MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentMonkeyCount, this);
            if (CharacterOwner->IsLocallyControlled())
            {
                OnRep_MonkeyCount();
            }
        }
        else
        {
            ResetThrowState_Server();
            Client_RejectThrow();
            return;
        }
    }

    const float ResetDelay = FMath::Max(0.2f, ActiveCooldown - DynamicMargin);
    GetWorld()->GetTimerManager().SetTimer(
        TimerHandle_ResetThrowState, this, &UThrowableComponent::ResetThrowState_Server, ResetDelay, false
    );
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

    if (CharacterOwner->CurrentWeapon)
    {
        TraceParams.AddIgnoredActor(CharacterOwner->CurrentWeapon);
    }

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

    if (CharacterOwner->CurrentWeapon)
    {
        QueryParams.AddIgnoredActor(CharacterOwner->CurrentWeapon);
    }

    const FCollisionShape SphereShape = FCollisionShape::MakeSphere(8.0f);
    if (GetWorld()->SweepSingleByChannel(HitResult, StartLoc, TargetLoc, FQuat::Identity, ECC_WorldStatic, SphereShape, QueryParams))
    {
        OutSpawnLoc = HitResult.Location + (HitResult.ImpactNormal * 2.0f);
    }
    else
    {
        OutSpawnLoc = TargetLoc;
    }
}

void UThrowableComponent::Server_OnGrenadeCookExpired()
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority()) return;

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(TimerHandle_CookExplosion);
        GetWorld()->GetTimerManager().ClearTimer(TimerHandle_ResetThrowState);
    }

    ChargeState = EThrowChargeState::Idle;
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, ChargeState, this);
    HandleChargeStateChanged();

    CurrentGrenadeCount = FMath::Max(0, CurrentGrenadeCount - 1);
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentGrenadeCount, this);
    if (CharacterOwner->IsLocallyControlled())
    {
        OnRep_GrenadeCount();
    }

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
        ECC_MAX
    );

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
        GetWorld()->GetTimerManager().ClearTimer(TimerHandle_LocalCookExplosion);
    }

    HandleChargeStateChanged();
}

void UThrowableComponent::ResetThrowState_Server()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(TimerHandle_CookExplosion);
        GetWorld()->GetTimerManager().ClearTimer(TimerHandle_ResetThrowState);
    }

    if (ChargeState != EThrowChargeState::Idle)
    {
        ChargeState = EThrowChargeState::Idle;
        MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, ChargeState, this);
        HandleChargeStateChanged();
    }
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
    if (ChargeState == EThrowChargeState::Idle) return;

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
    if (!AnimInstance) return;

    UAnimMontage* MontageToPlay = bIsMonkeyThrow ? MonkeyThrowMontage : GrenadeThrowMontage;

    switch (ChargeState)
    {
    case EThrowChargeState::Charging:
    {
        if (MontageToPlay)
        {
            PlayThrowMontageWithDelegate(MontageToPlay, HoldSectionName);
            AnimInstance->Montage_Pause(MontageToPlay);
        }
        SetWeaponHidden();
        ToggleHandThrowableVisibility(true);
        break;
    }
    case EThrowChargeState::Released:
    {
        if (MontageToPlay)
        {
            AnimInstance->Montage_Resume(MontageToPlay);
            AnimInstance->Montage_SetPlayRate(MontageToPlay, 1.0f);
            PlayThrowMontageWithDelegate(MontageToPlay, ReleaseSectionName);
        }
        ToggleHandThrowableVisibility(false);
        break;
    }
    case EThrowChargeState::Idle:
    default:
    {
        if (GrenadeThrowMontage && AnimInstance->GetActiveInstanceForMontage(GrenadeThrowMontage))
        {
            AnimInstance->Montage_Stop(0.05f, GrenadeThrowMontage);
        }
        if (MonkeyThrowMontage && AnimInstance->GetActiveInstanceForMontage(MonkeyThrowMontage))
        {
            AnimInstance->Montage_Stop(0.05f, MonkeyThrowMontage);
        }

        SetWeaponHidden();
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

    const bool bActive = (AnimInstance->GetActiveInstanceForMontage(MontageToPlay) != nullptr);

    if (!bActive)
    {
        AnimInstance->Montage_Play(MontageToPlay, 1.0f);
    }

    if (SectionName != NAME_None)
    {
        const FName CurrentSection = AnimInstance->Montage_GetCurrentSection(MontageToPlay);
        if (CurrentSection != SectionName)
        {
            AnimInstance->Montage_JumpToSection(SectionName, MontageToPlay);
        }
    }

    if (ChargeState == EThrowChargeState::Released)
    {
        FOnMontageEnded EndDelegate;
        EndDelegate.BindUObject(this, &UThrowableComponent::OnThrowMontageEnded);
        AnimInstance->Montage_SetEndDelegate(EndDelegate, MontageToPlay);
    }
}

void UThrowableComponent::ToggleHandThrowableVisibility(bool bVisible)
{
    if (IsRunningDedicatedServer() || !CharacterOwner) return;

    if (!bVisible)
    {
        if (CharacterOwner->MonkeyHandMesh)
        {
            CharacterOwner->MonkeyHandMesh->SetVisibility(false);
        }
        if (CharacterOwner->GrenadeHandMesh)
        {
            CharacterOwner->GrenadeHandMesh->SetVisibility(false);
        }
        return;
    }

    if (bIsMonkeyThrow)
    {
        if (CharacterOwner->GrenadeHandMesh) CharacterOwner->GrenadeHandMesh->SetVisibility(false);
        if (CharacterOwner->MonkeyHandMesh) CharacterOwner->MonkeyHandMesh->SetVisibility(true);
    }
    else
    {
        if (CharacterOwner->MonkeyHandMesh) CharacterOwner->MonkeyHandMesh->SetVisibility(false);
        if (CharacterOwner->GrenadeHandMesh) CharacterOwner->GrenadeHandMesh->SetVisibility(true);
    }
}

void UThrowableComponent::SetWeaponHidden()
{
    if (IsValid(CharacterOwner) && IsValid(CharacterOwner->CurrentWeapon))
    {
        CharacterOwner->UpdateWeaponVisibility();
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
        UThrowablePoolSubsystem* Pool = World ? World->GetSubsystem<UThrowablePoolSubsystem>() : nullptr;

        if (World && CharacterOwner && Pool)
        {
            const float CurrentTime = World->GetTimeSeconds();
            const bool bActualIsMonkey = bIsMonkeyThrow;
            uint8& TargetCount = bActualIsMonkey ? CurrentMonkeyCount : CurrentGrenadeCount;

            if (TargetCount > 0)
            {
                if (!bActualIsMonkey)
                {
                    World->GetTimerManager().ClearTimer(TimerHandle_CookExplosion);
                }

                const float ElapsedCookTime = CurrentTime - ServerCookStartTime;
                const float RemainingFuseTime = bActualIsMonkey ? 8.0f : FMath::Max(0.1f, MaxGrenadeCookTime - ElapsedCookTime);

                FVector DropLocation = CharacterOwner->GetActorLocation() + FVector(0.0f, 0.0f, 20.0f);
                FTransform DropTransform(CharacterOwner->GetActorRotation(), DropLocation);

                if (!bActualIsMonkey)
                {
                    if (AGrenade* DroppedGrenade = Pool->AcquireGrenade(GrenadeClass, DropTransform))
                    {
                        DroppedGrenade->SetOwner(CharacterOwner);
                        DroppedGrenade->SetInstigator(CharacterOwner);
                        DroppedGrenade->ActivateFromPool(
                            DropTransform,
                            FVector(FMath::RandRange(-80.0f, 80.0f), FMath::RandRange(-80.0f, 80.0f), 50.0f),
                            RemainingFuseTime,
                            CharacterOwner->GetController()
                        );

                        TargetCount--;
                        MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentGrenadeCount, this);
                    }
                }
                else
                {
                    if (AMonkeyBomb* DroppedMonkey = Pool->AcquireMonkey(MonkeyClass, DropTransform))
                    {
                        DroppedMonkey->SetOwner(CharacterOwner);
                        DroppedMonkey->SetInstigator(CharacterOwner);
                        DroppedMonkey->ActivateFromPool(
                            DropTransform,
                            FVector(0.0f, 0.0f, 30.0f),
                            8.0f,
                            CharacterOwner->GetController()
                        );

                        TargetCount--;
                        MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentMonkeyCount, this);
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