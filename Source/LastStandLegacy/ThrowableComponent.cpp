#include "ThrowableComponent.h"
#include "Hama.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"

UThrowableComponent::UThrowableComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);

    bIsCharging = false;
    bIsThrowingInProcess = false;
    LastThrowTime = -100.0f;
    ThrowCooldown = 1.0f;

    MaxGrenadeCount = 5;
    CurrentGrenadeCount = 5;

    MaxMonkeyCount = 3;
    CurrentMonkeyCount = 0;
}

void UThrowableComponent::BeginPlay()
{
    Super::BeginPlay();
    CharacterOwner = Cast<AHama>(GetOwner());

    if (GrenadeThrowMontage)
    {
        int32 SectionIndex = GrenadeThrowMontage->GetSectionIndex(ReleaseSectionName);
        ThrowCooldown = (SectionIndex != INDEX_NONE) ? GrenadeThrowMontage->GetSectionLength(SectionIndex) : GrenadeThrowMontage->GetPlayLength();
    }
}

void UThrowableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Params;
    Params.bIsPushBased = true;

    DOREPLIFETIME_WITH_PARAMS(UThrowableComponent, CurrentGrenadeCount, Params);
    DOREPLIFETIME_WITH_PARAMS(UThrowableComponent, CurrentMonkeyCount, Params);
    DOREPLIFETIME_WITH_PARAMS(UThrowableComponent, bIsCharging, Params);
}

void UThrowableComponent::StartGrenadeCharge() { Internal_StartCharge(GrenadeClass, CurrentGrenadeCount, GrenadeThrowMontage); }
void UThrowableComponent::StartMonkeyCharge() { Internal_StartCharge(MonkeyClass, CurrentMonkeyCount, MonkeyThrowMontage); }

void UThrowableComponent::Internal_StartCharge(TSubclassOf<AActor> ThrowableClass, int32 CurrentCount, UAnimMontage* MontageToPlay)
{
    if (!CharacterOwner || CharacterOwner->IsSwappingWeapon() || !ThrowableClass || CurrentCount <= 0 || bIsCharging || bIsThrowingInProcess) return;

    const float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastThrowTime < ThrowCooldown) return;

    bIsCharging = true;
    bIsThrowingInProcess = true;
    SetWeaponHidden(true);

    if (MontageToPlay && CharacterOwner->GetMesh())
    {
        if (UAnimInstance* AnimInstance = CharacterOwner->GetMesh()->GetAnimInstance())
        {
            AnimInstance->Montage_Play(MontageToPlay);
            AnimInstance->Montage_JumpToSection(HoldSectionName, MontageToPlay);
            AnimInstance->Montage_SetPlayRate(MontageToPlay, 0.0f);
        }
    }

    if (!GetOwner()->HasAuthority())
    {
        Server_StartCharge();
    }
}

void UThrowableComponent::Server_StartCharge_Implementation()
{
    const float CurrentTime = GetWorld()->GetTimeSeconds();
    if (!CharacterOwner || CharacterOwner->IsSwappingWeapon() || (CurrentTime - LastThrowTime < (ThrowCooldown - 0.05f)) || bIsThrowingInProcess)
    {
        Client_ResetThrowState();
        return;
    }

    bIsCharging = true;
    bIsThrowingInProcess = true;
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, bIsCharging, this);
}

void UThrowableComponent::ReleaseGrenadeThrow() { Internal_ReleaseThrow(GrenadeClass, CurrentGrenadeCount, GrenadeThrowMontage, false); }
void UThrowableComponent::ReleaseMonkeyThrow() { Internal_ReleaseThrow(MonkeyClass, CurrentMonkeyCount, MonkeyThrowMontage, true); }

void UThrowableComponent::Internal_ReleaseThrow(TSubclassOf<AActor> ThrowableClass, int32 CurrentCount, UAnimMontage* MontageToPlay, bool bIsMonkey)
{
    if (!CharacterOwner || !bIsCharging) return;

    LastThrowTime = GetWorld()->GetTimeSeconds();

    if (MontageToPlay && CharacterOwner->GetMesh())
    {
        if (UAnimInstance* AnimInstance = CharacterOwner->GetMesh()->GetAnimInstance())
        {
            AnimInstance->Montage_SetPlayRate(MontageToPlay, 1.0f);
            AnimInstance->Montage_JumpToSection(ReleaseSectionName, MontageToPlay);

            FOnMontageEnded EndedDelegate;
            EndedDelegate.BindUObject(this, &UThrowableComponent::OnThrowMontageEnded);
            AnimInstance->Montage_SetEndDelegate(EndedDelegate, MontageToPlay);
        }
    }

    FVector AimDir = GetCameraAimDirection();
    Server_ExecuteThrow(AimDir, bIsMonkey);
}

void UThrowableComponent::Server_ExecuteThrow_Implementation(FVector_NetQuantizeNormal LaunchDirection, bool bIsMonkey)
{
    TSubclassOf<AActor> TargetClass = bIsMonkey ? MonkeyClass : GrenadeClass;
    int32& TargetCount = bIsMonkey ? CurrentMonkeyCount : CurrentGrenadeCount;

    if (!CharacterOwner || !TargetClass || TargetCount <= 0 || !bIsCharging)
    {
        ResetThrowState_Server();
        Client_ResetThrowState();
        return;
    }

    LastThrowTime = GetWorld()->GetTimeSeconds();
    bIsCharging = false;
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, bIsCharging, this);

    FVector ServerAimDir = CharacterOwner->GetBaseAimRotation().Vector();
    FVector ValidatedAimDir = LaunchDirection.GetSafeNormal();

    if (FVector::DotProduct(ValidatedAimDir, ServerAimDir) < 0.5f)
    {
        ValidatedAimDir = ServerAimDir;
    }

    FVector TargetPoint = GetCrosshairTargetPoint(ValidatedAimDir);
    FVector PawnViewLoc = CharacterOwner->GetPawnViewLocation();
    FVector StartLoc = PawnViewLoc + (ValidatedAimDir * 40.0f);

    FVector FinalSpawnLoc;
    GetSafeSpawnLocation(PawnViewLoc, StartLoc, FinalSpawnLoc);
    FVector TrueLaunchDir = (TargetPoint - FinalSpawnLoc).GetSafeNormal();

    TargetCount--;
    if (bIsMonkey)
    {
        MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentMonkeyCount, this);
        OnRep_MonkeyCount();
    }
    else
    {
        MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, CurrentGrenadeCount, this);
        OnRep_GrenadeCount();
    }

    FRotator SpawnRotation = TrueLaunchDir.Rotation();
    FTransform SpawnTransform(SpawnRotation, FinalSpawnLoc);

    AActor* SpawnedThrowable = GetWorld()->SpawnActorDeferred<AActor>(
        TargetClass, SpawnTransform, CharacterOwner, CharacterOwner, ESpawnActorCollisionHandlingMethod::AlwaysSpawn
    );

    if (SpawnedThrowable)
    {
        if (UProjectileMovementComponent* ProjComp = SpawnedThrowable->FindComponentByClass<UProjectileMovementComponent>())
        {
            ProjComp->bInitialVelocityInLocalSpace = false;
            ProjComp->MaxSpeed = FMath::Max(ProjComp->MaxSpeed, ThrowImpulseStrength);
            ProjComp->Velocity = TrueLaunchDir * ThrowImpulseStrength;
        }
        UGameplayStatics::FinishSpawningActor(SpawnedThrowable, SpawnTransform);
    }

    GetWorld()->GetTimerManager().SetTimer(
        TimerHandle_ResetThrowState, this, &UThrowableComponent::ResetThrowState_Server, ThrowCooldown, false
    );
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

void UThrowableComponent::ResetThrowState_Server()
{
    bIsCharging = false;
    bIsThrowingInProcess = false;
    MARK_PROPERTY_DIRTY_FROM_NAME(UThrowableComponent, bIsCharging, this);
    SetWeaponHidden(false);
}

void UThrowableComponent::Client_ResetThrowState_Implementation()
{
    bIsCharging = false;
    bIsThrowingInProcess = false;
    SetWeaponHidden(false);
}

void UThrowableComponent::OnThrowMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    bIsThrowingInProcess = false;
    SetWeaponHidden(false);
}

void UThrowableComponent::OnRep_IsCharging() { SetWeaponHidden(bIsCharging); }

void UThrowableComponent::SetWeaponHidden(bool bHidden)
{
    if (CharacterOwner && IsValid(CharacterOwner->CurrentWeapon))
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

    if (CharacterOwner->CurrentWeapon)
    {
        TraceParams.AddIgnoredActor(CharacterOwner->CurrentWeapon);
    }

    if (GetWorld() && GetWorld()->LineTraceSingleByChannel(Hit, CameraLoc, TraceEnd, ECC_Visibility, TraceParams))
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

    if (GetWorld()->LineTraceSingleByChannel(HitResult, StartLoc, TargetLoc, ECC_Visibility, QueryParams))
    {
        OutSpawnLoc = HitResult.Location - (TargetLoc - StartLoc).GetSafeNormal() * 10.0f;
    }
    else
    {
        OutSpawnLoc = TargetLoc;
    }
}