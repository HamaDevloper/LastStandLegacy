#include "MonkeyBomb.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "ZombieDirectorSubsystem.h"
#include "ThrowablePoolSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

AMonkeyBomb::AMonkeyBomb()
{
    PrimaryActorTick.bCanEverTick = false;

    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicateMovement(false);

    SetNetUpdateFrequency(10.0f);
    SetMinNetUpdateFrequency(2.0f);

    bAttractionActivated = false;
    bHasExploded = false;
    SettledLocation = FVector::ZeroVector;
    ExplosionLocation = FVector::ZeroVector;

    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
    CollisionComp->InitSphereRadius(8.0f);
    CollisionComp->SetCollisionProfileName(TEXT("Projectile"));
    CollisionComp->SetCanEverAffectNavigation(false);
    CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    CollisionComp->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
    CollisionComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    CollisionComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
    RootComponent = CollisionComp;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(RootComponent);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComp->SetCastShadow(true);

    AttractionAudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("AttractionAudioComp"));
    AttractionAudioComp->SetupAttachment(RootComponent);
    AttractionAudioComp->bAutoActivate = false;

    ProjectileMovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComp"));
    ProjectileMovementComp->UpdatedComponent = RootComponent;
    ProjectileMovementComp->InitialSpeed = 0.0f;
    ProjectileMovementComp->MaxSpeed = 10000.0f;
    ProjectileMovementComp->bInitialVelocityInLocalSpace = false;
    ProjectileMovementComp->bAutoActivate = false;
    ProjectileMovementComp->Velocity = FVector::ZeroVector;
    ProjectileMovementComp->bRotationFollowsVelocity = false;
    ProjectileMovementComp->bShouldBounce = true;
    ProjectileMovementComp->Bounciness = 0.15f;
    ProjectileMovementComp->Friction = 0.7f;
    ProjectileMovementComp->BounceVelocityStopSimulatingThreshold = 5.0f;
}

void AMonkeyBomb::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Params;
    Params.bIsPushBased = true;

    DOREPLIFETIME_WITH_PARAMS_FAST(AMonkeyBomb, LaunchData, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AMonkeyBomb, SettledLocation, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AMonkeyBomb, bAttractionActivated, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AMonkeyBomb, bHasExploded, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AMonkeyBomb, ExplosionLocation, Params);
}

void AMonkeyBomb::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        CachedInstigatorController = GetInstigatorController();

        if (ProjectileMovementComp)
        {
            ProjectileMovementComp->OnProjectileStop.AddUniqueDynamic(this, &AMonkeyBomb::OnProjectileStopped);
        }
    }
    else
    {
        // چارەسەری سەرەکی: کاتێک مەیموونەکە بۆ یەکەم جار لە Poolـەوە دەگاتە کڵایەنت،
        // OnRep_LaunchData پێش BeginPlay بانگ دەکرێت (کاتێک کۆمپۆنێنتەکان هێشتا Register نەبوون).
        // لێرەدا دوای Register بوونی تەواوی کۆمپۆنێنتەکان فڕینەکە چالاک دەکەین.
        if (LaunchData.LaunchSpeed > 0.0f && !bHasExploded)
        {
            ApplyLaunchData();
        }
    }
}

void AMonkeyBomb::IgnoreOwnerAndAttachedActors(AActor* InOwnerActor)
{
    if (!CollisionComp) return;

    CollisionComp->ClearMoveIgnoreActors();

    auto AddActorAndChildrenToIgnore = [this](AActor* TargetActor)
        {
            if (!TargetActor) return;
            CollisionComp->IgnoreActorWhenMoving(TargetActor, true);

            TArray<AActor*> AttachedActors;
            TargetActor->GetAttachedActors(AttachedActors);
            for (AActor* Attached : AttachedActors)
            {
                if (Attached)
                {
                    CollisionComp->IgnoreActorWhenMoving(Attached, true);
                }
            }
        };

    AddActorAndChildrenToIgnore(InOwnerActor);

    if (!HasAuthority() && GetWorld())
    {
        if (APlayerController* LocalPC = GetWorld()->GetFirstPlayerController())
        {
            if (APawn* LocalPawn = LocalPC->GetPawn())
            {
                AddActorAndChildrenToIgnore(LocalPawn);
            }
        }
    }
}

void AMonkeyBomb::InitVelocity(const FVector& LaunchVelocity)
{
    if (!HasAuthority()) return;

    if (ProjectileMovementComp)
    {
        ProjectileMovementComp->StopMovementImmediately();
        ProjectileMovementComp->SetUpdatedComponent(CollisionComp);
        ProjectileMovementComp->Velocity = LaunchVelocity;
        ProjectileMovementComp->UpdateComponentVelocity();
        ProjectileMovementComp->Activate(true);
        ProjectileMovementComp->SetComponentTickEnabled(true);
    }
}

void AMonkeyBomb::OnRep_LaunchData()
{
    if (!HasActorBegunPlay())
    {
        return;
    }

    ApplyLaunchData();
}

void AMonkeyBomb::ApplyLaunchData()
{
    if (LaunchData.LaunchSpeed <= 0.0f) return;

    bHasExploded = false;

    AActor* OwnerActor = GetOwner() ? GetOwner() : GetInstigator();
    IgnoreOwnerAndAttachedActors(OwnerActor);

    const FVector BaseDirection = LaunchData.LaunchDirection;
    const float BaseSpeed = LaunchData.LaunchSpeed;
    FVector InitialVel = BaseDirection * BaseSpeed;
    FVector InitialLoc = LaunchData.SpawnLocation;

    // =========================================================================
    // قەرەبووکردنەوەی پینگ (Fast-Forward Simulation) بۆ کڵایەنت
    // =========================================================================
    if (!HasAuthority())
    {
        float PingSeconds = 0.03f;
        if (APawn* InstigatorPawn = GetInstigator())
        {
            if (APlayerState* PS = InstigatorPawn->GetPlayerState())
            {
                // نیوەی کاتی هاتووچۆ (One-Way Ping) بە چرکە
                PingSeconds = FMath::Clamp(PS->GetPingInMilliseconds() * 0.0005f, 0.01f, 0.15f);
            }
        }

        // هاوکێشەی فیزیکی کێشکردن بۆ تێپەڕاندنی ئەو کاتەی پاکێت لە ڕێگادا بووە:
        // P = P0 + (V * t) + (0.5 * a * t^2)
        const FVector Gravity(0.0f, 0.0f, GetWorld() ? GetWorld()->GetGravityZ() : -980.0f);
        InitialLoc += (InitialVel * PingSeconds) + (0.5f * Gravity * FMath::Square(PingSeconds));
        InitialVel += (Gravity * PingSeconds);
    }

    SetActorLocationAndRotation(
        InitialLoc,
        BaseDirection.Rotation(),
        false,
        nullptr,
        ETeleportType::ResetPhysics
    );

    SetActorHiddenInGame(false);
    if (MeshComp)
    {
        MeshComp->SetVisibility(true, true);
    }
    SetActorEnableCollision(true);

    if (ProjectileMovementComp)
    {
        ProjectileMovementComp->StopMovementImmediately();
        ProjectileMovementComp->SetUpdatedComponent(CollisionComp);
        ProjectileMovementComp->Velocity = InitialVel;
        ProjectileMovementComp->UpdateComponentVelocity();
        ProjectileMovementComp->Activate(true);
        ProjectileMovementComp->SetComponentTickEnabled(true);
    }
}

void AMonkeyBomb::OnProjectileStopped(const FHitResult& ImpactResult)
{
    if (!HasAuthority() || bHasExploded) return;
    ActivateAttraction();
}

void AMonkeyBomb::ActivateAttraction()
{
    if (!HasAuthority() || bAttractionActivated || bHasExploded) return;

    bAttractionActivated = true;
    MARK_PROPERTY_DIRTY_FROM_NAME(AMonkeyBomb, bAttractionActivated, this);

    SettledLocation = GetActorLocation();
    MARK_PROPERTY_DIRTY_FROM_NAME(AMonkeyBomb, SettledLocation, this);

    if (ProjectileMovementComp)
    {
        ProjectileMovementComp->StopMovementImmediately();
        ProjectileMovementComp->Deactivate();
    }

    if (UZombieDirectorSubsystem* Director = GetWorld()->GetSubsystem<UZombieDirectorSubsystem>())
    {
        Director->RegisterAttractor(this, AttractionRadius);
    }

    OnRep_AttractionActivated();
    ForceNetUpdate();
    GetWorldTimerManager().SetTimer(FuseTimerHandle, this, &AMonkeyBomb::Explode, FuseDuration, false);
}

void AMonkeyBomb::OnRep_SettledLocation()
{
    if (bHasExploded || SettledLocation.IsNearlyZero())
    {
        return;
    }

    if (ProjectileMovementComp)
    {
        ProjectileMovementComp->StopMovementImmediately();
        ProjectileMovementComp->Deactivate();
    }

    SetActorLocation(SettledLocation, false, nullptr, ETeleportType::TeleportPhysics);
}

void AMonkeyBomb::OnRep_AttractionActivated()
{
    if (bAttractionActivated && !bHasExploded)
    {
        if (AttractionSound && AttractionAudioComp)
        {
            if (!AttractionAudioComp->IsPlaying())
            {
                AttractionAudioComp->SetSound(AttractionSound);
                AttractionAudioComp->Play();
            }
        }
    }
    else
    {
        if (AttractionAudioComp && AttractionAudioComp->IsPlaying())
        {
            AttractionAudioComp->Stop();
        }
    }
}

float AMonkeyBomb::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (!HasAuthority() || bHasExploded || DamageAmount <= 0.0f) return 0.0f;
    Explode();
    return DamageAmount;
}

void AMonkeyBomb::Explode()
{
    if (!HasAuthority() || bHasExploded) return;

    bHasExploded = true;
    bAttractionActivated = false;
    ExplosionLocation = GetActorLocation();
    SettledLocation = FVector::ZeroVector;

    MARK_PROPERTY_DIRTY_FROM_NAME(AMonkeyBomb, bHasExploded, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AMonkeyBomb, bAttractionActivated, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AMonkeyBomb, ExplosionLocation, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AMonkeyBomb, SettledLocation, this);

    SetNetUpdateFrequency(10.0f);
    ForceNetUpdate();
    GetWorldTimerManager().ClearTimer(FuseTimerHandle);

    if (ProjectileMovementComp)
    {
        ProjectileMovementComp->StopMovementImmediately();
        ProjectileMovementComp->Deactivate();
    }

    if (UZombieDirectorSubsystem* Director = GetWorld()->GetSubsystem<UZombieDirectorSubsystem>())
    {
        Director->UnregisterAttractor(this);
    }

    PlayExplosionFX(ExplosionLocation);

    AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
    TArray<AActor*> IgnoredActors;
    IgnoredActors.Reserve(GameState ? GameState->PlayerArray.Num() + 1 : 4);
    IgnoredActors.Add(this);

    APawn* InstigatorPawn = GetInstigator();
    const bool bIsThrowerAlive = (InstigatorPawn != nullptr);

    if (GameState)
    {
        for (APlayerState* PS : GameState->PlayerArray)
        {
            if (!PS) continue;
            if (APawn* PlayerPawn = PS->GetPawn())
            {
                if (!bIsThrowerAlive || PlayerPawn != InstigatorPawn)
                {
                    IgnoredActors.Add(PlayerPawn);
                }
            }
        }
    }

    AController* DamageInstigator = CachedInstigatorController.IsValid()
        ? CachedInstigatorController.Get()
        : GetInstigatorController();

    UGameplayStatics::ApplyRadialDamage(
        this,
        BaseDamage,
        ExplosionLocation,
        DamageRadius,
        UDamageType::StaticClass(),
        IgnoredActors,
        this,
        DamageInstigator,
        true,
        ECC_MAX
    );

    SetActorHiddenInGame(true);
    if (MeshComp)
    {
        MeshComp->SetVisibility(false, true);
    }
    SetActorEnableCollision(false);

    GetWorldTimerManager().SetTimer(
        ReturnToPoolTimerHandle,
        this,
        &AMonkeyBomb::PrepareForPool,
        0.3f,
        false
    );
}

void AMonkeyBomb::OnRep_HasExploded()
{
    if (bHasExploded)
    {
        PlayExplosionFX(ExplosionLocation);
        SetActorHiddenInGame(true);
        if (MeshComp)
        {
            MeshComp->SetVisibility(false, true);
        }
        SetActorEnableCollision(false);

        if (ProjectileMovementComp)
        {
            ProjectileMovementComp->StopMovementImmediately();
            ProjectileMovementComp->Deactivate();
        }
    }
}

void AMonkeyBomb::PlayExplosionFX(const FVector& Location)
{
    if (GetNetMode() == NM_DedicatedServer) return;

    if (AttractionAudioComp && AttractionAudioComp->IsPlaying())
    {
        AttractionAudioComp->Stop();
    }

    if (ExplosionSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, Location);
    }

    if (ExplosionNiagaraFX && GetWorld())
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(),
            ExplosionNiagaraFX,
            Location,
            FRotator::ZeroRotator,
            FVector(1.0f),
            true,
            true,
            ENCPoolMethod::AutoRelease
        );
    }
}

void AMonkeyBomb::ActivateFromPool(const FTransform& SpawnTransform, const FVector& LaunchVelocity, float InFuseDuration, AController* InInstigatorController)
{
    if (!HasAuthority()) return;

    GetWorldTimerManager().ClearTimer(ReturnToPoolTimerHandle);
    GetWorldTimerManager().ClearTimer(FuseTimerHandle);

    SetNetDormancy(DORM_Awake);
    FlushNetDormancy();
    SetNetUpdateFrequency(10.0f);

    if (InInstigatorController && InInstigatorController->GetPawn())
    {
        SetOwner(InInstigatorController->GetPawn());
        SetInstigator(InInstigatorController->GetPawn());
    }

    bHasExploded = false;
    bAttractionActivated = false;
    SettledLocation = FVector::ZeroVector;
    CachedInstigatorController = InInstigatorController;
    FuseDuration = FMath::Max(0.1f, InFuseDuration);

    LaunchData.SpawnLocation = SpawnTransform.GetLocation();
    LaunchData.LaunchDirection = LaunchVelocity.GetSafeNormal();
    LaunchData.LaunchSpeed = LaunchVelocity.Size();
    LaunchData.LaunchIndex++;

    IgnoreOwnerAndAttachedActors(GetOwner() ? GetOwner() : GetInstigator());

    SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::ResetPhysics);
    SetActorHiddenInGame(false);
    if (MeshComp)
    {
        MeshComp->SetVisibility(true, true);
    }
    SetActorEnableCollision(true);

    MARK_PROPERTY_DIRTY_FROM_NAME(AMonkeyBomb, bHasExploded, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AMonkeyBomb, bAttractionActivated, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AMonkeyBomb, SettledLocation, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AMonkeyBomb, LaunchData, this);

    InitVelocity(LaunchVelocity);

    ForceNetUpdate();
}

void AMonkeyBomb::PrepareForPool()
{
    if (!HasAuthority()) return;

    SetActorHiddenInGame(true);
    if (MeshComp)
    {
        MeshComp->SetVisibility(false, true);
    }
    SetActorEnableCollision(false);

    if (CollisionComp)
    {
        CollisionComp->ClearMoveIgnoreActors();
    }

    if (AttractionAudioComp && AttractionAudioComp->IsPlaying())
    {
        AttractionAudioComp->Stop();
    }

    if (ProjectileMovementComp)
    {
        ProjectileMovementComp->StopMovementImmediately();
        ProjectileMovementComp->Deactivate();
    }

    if (bAttractionActivated)
    {
        bAttractionActivated = false;
        MARK_PROPERTY_DIRTY_FROM_NAME(AMonkeyBomb, bAttractionActivated, this);
    }

    if (UZombieDirectorSubsystem* Director = GetWorld()->GetSubsystem<UZombieDirectorSubsystem>())
    {
        Director->UnregisterAttractor(this);
    }

    GetWorldTimerManager().ClearTimer(FuseTimerHandle);
    GetWorldTimerManager().ClearTimer(ReturnToPoolTimerHandle);

    FlushNetDormancy();
    SetNetDormancy(DORM_DormantAll);

    if (UThrowablePoolSubsystem* Pool = GetWorld()->GetSubsystem<UThrowablePoolSubsystem>())
    {
        Pool->ReturnMonkey(this);
    }
}