#include "ThrowableComponent.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Hama.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Grenade.h"
#include "LastStandLegacyGameState.h"
#include "GameFramework/PlayerState.h"

UThrowableComponent::UThrowableComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
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

    Debug_RenderNetworkDesync();
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

    bool bDesyncDetected = false;
    if (!bIsServer && bIsThrowingLocal && ChargeState == EThrowChargeState::Idle)
    {
        bDesyncDetected = true;
    }

    FColor DisplayColor = bDesyncDetected ? FColor::Red :
        (bIsOnCooldown ? FColor::Yellow : (bIsServer ? FColor::Green : FColor::Cyan));

    FString DebugMsg = FString::Printf(
        TEXT("[%s] Grenades: %d/%d | Monkeys: %d/%d | Cooldown: %.2fs | State: %d | LocalThrow: %s %s"),
        *RoleName,
        CurrentGrenadeCount, MaxGrenadeCount,
        CurrentMonkeyCount, MaxMonkeyCount,
        RemainingCooldown,
        static_cast<int32>(ChargeState),
        bIsThrowingLocal ? TEXT("TRUE") : TEXT("FALSE"),
        bDesyncDetected ? TEXT(" | [DESYNC DETECTED!]") : TEXT("")
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
        int32 SectionIndex = GrenadeThrowMontage->GetSectionIndex(ReleaseSectionName);
        GrenadeThrowCooldown = (SectionIndex != INDEX_NONE) ? GrenadeThrowMontage->GetSectionLength(SectionIndex) : GrenadeThrowMontage->GetPlayLength();
    }

    if (MonkeyThrowMontage)
    {
        int32 SectionIndex = MonkeyThrowMontage->GetSectionIndex(ReleaseSectionName);
        MonkeyThrowCooldown = (SectionIndex != INDEX_NONE) ? MonkeyThrowMontage->GetSectionLength(SectionIndex) : MonkeyThrowMontage->GetPlayLength();
    }
}

void UThrowableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams RepParams;
    RepParams.bIsPushBased = true;

    RepParams.Condition = COND_None;
    DOREPLIFETIME_WITH_PARAMS(UThrowableComponent, ChargeState, RepParams);
    DOREPLIFETIME_WITH_PARAMS(UThrowableComponent, bIsMonkeyThrow, RepParams);

    RepParams.Condition = COND_OwnerOnly;
    DOREPLIFETIME_WITH_PARAMS(UThrowableComponent, CurrentGrenadeCount, RepParams);
    DOREPLIFETIME_WITH_PARAMS(UThrowableComponent, CurrentMonkeyCount, RepParams);
}

void UThrowableComponent::StartGrenadeCharge() { Internal_StartCharge(GrenadeClass, CurrentGrenadeCount, GrenadeThrowMontage, false); }
void UThrowableComponent::StartMonkeyCharge() { Internal_StartCharge(MonkeyClass, CurrentMonkeyCount, MonkeyThrowMontage, true); }

void UThrowableComponent::Internal_StartCharge(TSubclassOf<AActor> ThrowableClass, int32 CurrentCount, UAnimMontage* MontageToPlay, bool bIsMonkey)
{
    if (!CharacterOwner || !ThrowableClass || CurrentCount <= 0 || IsCharging() || IsThrowingInProcess())
    {
        return;
    }

    const float ActiveCooldown = bIsMonkey ? MonkeyThrowCooldown : GrenadeThrowCooldown;
    const float CurrentTime = GetWorld()->GetTimeSeconds();

    if (CurrentTime - LastThrowTime < ActiveCooldown)
    {
        return;
    }

    bIsThrowingLocal = true;
    bIsMonkeyThrow = bIsMonkey;

    if (!GetOwner()->HasAuthority())
    {
        if (!bIsMonkey)
        {
            GetWorld()->GetTimerManager().SetTimer(
                TimerHandle_CookExplosion,
                this,
                &UThrowableComponent::Multicast_OnGrenadeCookExpiredFX,
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
    ChargeState = EThrowChargeState::Charging;
    bIsMonkeyThrow = bIsMonkey;

    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, bIsMonkeyThrow, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, ChargeState, this);

    HandleChargeStateChanged();

    if (!bIsMonkeyThrow)
    {
        ServerCookStartTime = GetWorld()->GetTimeSeconds();

        GetWorld()->GetTimerManager().SetTimer(
            TimerHandle_CookExplosion,
            this,
            &UThrowableComponent::Server_OnGrenadeCookExpired,
            MaxGrenadeCookTime,
            false
        );
    }
}

void UThrowableComponent::Server_StartCharge_Implementation(bool bIsMonkey)
{
    const int32 TargetCount = bIsMonkey ? CurrentMonkeyCount : CurrentGrenadeCount;
    const TSubclassOf<AActor> TargetClass = bIsMonkey ? MonkeyClass : GrenadeClass;
    const float ActiveCooldown = bIsMonkey ? MonkeyThrowCooldown : GrenadeThrowCooldown;
    const float CurrentTime = GetWorld()->GetTimeSeconds();

    if (!CharacterOwner || !TargetClass || TargetCount <= 0 || (CurrentTime - LastThrowTime < (ActiveCooldown - 0.05f)) || ChargeState != EThrowChargeState::Idle)
    {
        ResetThrowState_Server();
        return;
    }

    ExecuteStartCharge_Server(bIsMonkey);
}

void UThrowableComponent::ReleaseGrenadeThrow() { Internal_ReleaseThrow(GrenadeClass, CurrentGrenadeCount, GrenadeThrowMontage, false); }
void UThrowableComponent::ReleaseMonkeyThrow() { Internal_ReleaseThrow(MonkeyClass, CurrentMonkeyCount, MonkeyThrowMontage, true); }

void UThrowableComponent::Internal_ReleaseThrow(TSubclassOf<AActor> ThrowableClass, int32 CurrentCount, UAnimMontage* MontageToPlay, bool bIsMonkey)
{
    if (!CharacterOwner || ChargeState != EThrowChargeState::Charging)
    {
        return;
    }

    ChargeState = EThrowChargeState::Released;
    HandleChargeStateChanged();

    FVector AimDir = GetCameraAimDirection();

    Server_ExecuteThrow(AimDir, bIsMonkey);

    if (!GetOwner()->HasAuthority())
    {
        LastThrowTime = GetWorld()->GetTimeSeconds();
    }
}

void UThrowableComponent::Server_ExecuteThrow_Implementation(FVector_NetQuantizeNormal LaunchDirection, bool bIsMonkey)
{
    const bool bActualIsMonkey = bIsMonkeyThrow;

    if (!bActualIsMonkey && GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(TimerHandle_CookExplosion);
    }

    TSubclassOf<AActor> TargetClass = bActualIsMonkey ? MonkeyClass : GrenadeClass;
    int32& TargetCount = bActualIsMonkey ? CurrentMonkeyCount : CurrentGrenadeCount;
    const float ActiveCooldown = bActualIsMonkey ? MonkeyThrowCooldown : GrenadeThrowCooldown;

    const float CurrentTime = GetWorld()->GetTimeSeconds();
    const bool bIsCooldownActive = (CurrentTime - LastThrowTime) < (ActiveCooldown - 0.05f);

    const bool bValidState = (ChargeState == EThrowChargeState::Charging) ||
        (CharacterOwner && CharacterOwner->HasAuthority() && ChargeState == EThrowChargeState::Released);

    if (!CharacterOwner || !TargetClass || TargetCount <= 0 || !bValidState || bIsCooldownActive)
    {
        ResetThrowState_Server();
        return;
    }

    LastThrowTime = CurrentTime;
    ChargeState = EThrowChargeState::Released;

    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, ChargeState, this);
    HandleChargeStateChanged();

    FVector ServerAimDir = CharacterOwner->GetBaseAimRotation().Vector();
    FVector ValidatedAimDir = LaunchDirection.GetSafeNormal();

    if (FVector::DotProduct(ValidatedAimDir, ServerAimDir) < 0.7f)
    {
        ValidatedAimDir = ServerAimDir;
    }

    FVector TargetPoint = GetCrosshairTargetPoint(ValidatedAimDir);
    FVector PawnViewLoc = CharacterOwner->GetPawnViewLocation();
    FVector StartLoc = PawnViewLoc + (ValidatedAimDir * 40.0f);

    FVector FinalSpawnLoc;
    GetSafeSpawnLocation(PawnViewLoc, StartLoc, FinalSpawnLoc);
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
        }

        if (UProjectileMovementComponent* ProjComp = SpawnedThrowable->FindComponentByClass<UProjectileMovementComponent>())
        {
            ProjComp->bInitialVelocityInLocalSpace = false;
            ProjComp->MaxSpeed = FMath::Max(ProjComp->MaxSpeed, ThrowImpulseStrength);
            ProjComp->Velocity = TrueLaunchDir * ThrowImpulseStrength;
        }

        UGameplayStatics::FinishSpawningActor(SpawnedThrowable, SpawnTransform);

        TargetCount--;
        if (bActualIsMonkey)
        {
            MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentMonkeyCount, this);
            OnRep_MonkeyCount();
        }
        else
        {
            MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentGrenadeCount, this);
            OnRep_GrenadeCount();
        }
    }
    else
    {
        ResetThrowState_Server();
        return;
    }

    GetWorld()->GetTimerManager().SetTimer(
        TimerHandle_ResetThrowState, this, &UThrowableComponent::ResetThrowState_Server, ActiveCooldown, false
    );
}

void UThrowableComponent::Server_OnGrenadeCookExpired()
{
    if (!CharacterOwner || !CharacterOwner->HasAuthority()) return;

    ChargeState = EThrowChargeState::Idle;
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, ChargeState, this);

    HandleChargeStateChanged();

    FVector ExpLocation = CharacterOwner->GetActorLocation();

    DrawDebugSphere(GetWorld(), ExpLocation, 400.0f, 16, FColor::Red, false, 3.0f, 0, 1.5f);

    TArray<AActor*> IgnoredActors;

    if (ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>())
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
                else
                {
                    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("OwnCharacter"));
                }
            }
        }
    }

    UGameplayStatics::ApplyRadialDamage(
        this,
        500.0f,
        ExpLocation,
        400.0f,
        UDamageType::StaticClass(),
        IgnoredActors,
        CharacterOwner,
        CharacterOwner->GetController(),
        true,
        ECC_WorldStatic
    );

    CurrentGrenadeCount = FMath::Max(0, CurrentGrenadeCount - 1);
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentGrenadeCount, this);
    OnRep_GrenadeCount();

    Multicast_OnGrenadeCookExpiredFX();
    ResetThrowState_Server();
}

void UThrowableComponent::Multicast_OnGrenadeCookExpiredFX_Implementation()
{
    bIsThrowingLocal = false;

    if (CharacterOwner && CharacterOwner->GetMesh())
    {
        if (UAnimInstance* AnimInst = CharacterOwner->GetMesh()->GetAnimInstance())
        {
            AnimInst->StopAllMontages(0.1f);
        }
    }
}

void UThrowableComponent::ResetThrowState_Server()
{
    ChargeState = EThrowChargeState::Idle;

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(TimerHandle_CookExplosion);
    }

    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, ChargeState, this);
    HandleChargeStateChanged();
}

void UThrowableComponent::OnThrowMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    ChargeState = EThrowChargeState::Idle;

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, ChargeState, this);
    }

    HandleChargeStateChanged();
}

void UThrowableComponent::OnRep_ChargeState()
{
    if (CharacterOwner && CharacterOwner->IsLocallyControlled())   return;
    HandleChargeStateChanged();
}

void UThrowableComponent::HandleChargeStateChanged()
{
    UAnimMontage* MontageToPlay = bIsMonkeyThrow ? MonkeyThrowMontage : GrenadeThrowMontage;
    if (!CharacterOwner || !CharacterOwner->GetMesh()) return;

    UAnimInstance* AnimInstance = CharacterOwner->GetMesh()->GetAnimInstance();

    switch (ChargeState)
    {
    case EThrowChargeState::Charging:
    {
        if (AnimInstance && MontageToPlay)
        {
            AnimInstance->Montage_Play(MontageToPlay);
            AnimInstance->Montage_JumpToSection(HoldSectionName, MontageToPlay);
            AnimInstance->Montage_SetPlayRate(MontageToPlay, 0.0f);

            FOnMontageEnded EndedDelegate;
            EndedDelegate.BindUObject(this, &UThrowableComponent::OnThrowMontageEnded);
            AnimInstance->Montage_SetEndDelegate(EndedDelegate, MontageToPlay);
        }

        SetWeaponHidden(true);
        ToggleHandThrowableVisibility(true);
        break;
    }
    case EThrowChargeState::Released:
    {
        if (AnimInstance && MontageToPlay)
        {
            AnimInstance->Montage_SetPlayRate(MontageToPlay, 1.0f);
            AnimInstance->Montage_JumpToSection(ReleaseSectionName, MontageToPlay);
        }
        SetWeaponHidden(false);
        ToggleHandThrowableVisibility(false);
        break;
    }
    case EThrowChargeState::Idle:
    default:
    {
        bIsThrowingLocal = false;

        if (GetWorld())
        {
            GetWorld()->GetTimerManager().ClearTimer(TimerHandle_CookExplosion);
        }

        if (AnimInstance && MontageToPlay && AnimInstance->Montage_IsPlaying(MontageToPlay))
        {
            AnimInstance->Montage_Stop(0.1f, MontageToPlay);
        }

        SetWeaponHidden(false);
        ToggleHandThrowableVisibility(false);
        break;
    }
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

FVector UThrowableComponent::GetCameraAimDirection() const
{
    if (!CharacterOwner) return FVector::ForwardVector;

    if (APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController()))
    {
        FVector CameraLoc;
        FRotator CameraRot;
        PC->GetPlayerViewPoint(CameraLoc, CameraRot);
        return CameraRot.Vector();
    }

    return CharacterOwner->GetBaseAimRotation().Vector();
}

FVector UThrowableComponent::GetCrosshairTargetPoint(const FVector& AimDir) const
{
    if (!CharacterOwner) return FVector::ZeroVector;

    FVector CameraLoc;
    FRotator CameraRot;

    if (APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController()))
    {
        PC->GetPlayerViewPoint(CameraLoc, CameraRot);
    }
    else
    {
        CameraLoc = CharacterOwner->GetPawnViewLocation();
    }

    FVector TraceEnd = CameraLoc + (AimDir * 10000.0f);
    FHitResult Hit;
    FCollisionQueryParams TraceParams;
    TraceParams.AddIgnoredActor(CharacterOwner);

    if (GetWorld() && GetWorld()->LineTraceSingleByChannel(Hit, CameraLoc, TraceEnd, ECC_WorldStatic, TraceParams))
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

void UThrowableComponent::OnRep_GrenadeCount() { OnThrowableCountChanged.Broadcast(CurrentMonkeyCount, CurrentGrenadeCount); }
void UThrowableComponent::OnRep_MonkeyCount() { OnThrowableCountChanged.Broadcast(CurrentMonkeyCount, CurrentGrenadeCount); }