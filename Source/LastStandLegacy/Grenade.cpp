#include "Grenade.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "ThrowablePoolSubsystem.h"

AGrenade::AGrenade()
{
    PrimaryActorTick.bCanEverTick = false;

    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicateMovement(false);

    SetNetUpdateFrequency(10.0f);
    SetMinNetUpdateFrequency(2.0f);

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

    ProjectileMovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComp"));
    ProjectileMovementComp->UpdatedComponent = RootComponent;
    ProjectileMovementComp->InitialSpeed = 0.0f;
    ProjectileMovementComp->MaxSpeed = 10000.0f;
    ProjectileMovementComp->bInitialVelocityInLocalSpace = false;
    ProjectileMovementComp->bAutoActivate = false;
    ProjectileMovementComp->Velocity = FVector::ZeroVector;
    ProjectileMovementComp->bRotationFollowsVelocity = false;
    ProjectileMovementComp->bShouldBounce = true;
    ProjectileMovementComp->Bounciness = 0.3f;
    ProjectileMovementComp->Friction = 0.5f;
    ProjectileMovementComp->BounceVelocityStopSimulatingThreshold = 5.0f;
}

void AGrenade::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Params;
    Params.bIsPushBased = true;

    DOREPLIFETIME_WITH_PARAMS_FAST(AGrenade, LaunchData, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AGrenade, SettledLocation, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AGrenade, bHasExploded, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AGrenade, ExplosionLocation, Params);
}

void AGrenade::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        CachedInstigatorController = GetInstigatorController();

        if (ProjectileMovementComp)
        {
            ProjectileMovementComp->OnProjectileStop.AddUniqueDynamic(this, &AGrenade::OnProjectileStopped);
        }
    }
    else
    {
        // ئەگەر ئەکتەرەکە بۆ یەکەم جار لەسەر کڵایەنت دروست بووبێت و OnRep پێش BeginPlay گەیشتبێت
        if (LaunchData.LaunchSpeed > 0.0f && !bHasExploded)
        {
            ApplyLaunchData();
        }
    }
}

void AGrenade::SetFuseDuration(float NewDuration)
{
    FuseDuration = FMath::Max(0.1f, NewDuration);
}

void AGrenade::IgnoreOwnerAndAttachedActors(AActor* InOwnerActor)
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

    // لەسەر کڵایەنت ئەگەر هێشتا Owner نەگەیشتبوو، کارەکتەری لۆکاڵیش پشتگوێ دەخەین تا لە دەستیدا گیر نەخوات
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

void AGrenade::InitVelocity(const FVector& LaunchVelocity)
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

void AGrenade::OnRep_LaunchData()
{
    if (!HasActorBegunPlay())
    {
        return;
    }

    ApplyLaunchData();
}

void AGrenade::ApplyLaunchData()
{
    if (LaunchData.LaunchSpeed <= 0.0f) return;

    bHasExploded = false;

    AActor* OwnerActor = GetOwner() ? GetOwner() : GetInstigator();
    IgnoreOwnerAndAttachedActors(OwnerActor);

    const FVector BaseDirection = LaunchData.LaunchDirection;
    const float BaseSpeed = LaunchData.LaunchSpeed;
    FVector InitialVel = BaseDirection * BaseSpeed;
    FVector InitialLoc = LaunchData.SpawnLocation;

    // قەرەبووکردنەوەی پینگ (Fast-Forward) تەنیا لەسەر کڵایەنت
    if (!HasAuthority())
    {
        float PingSeconds = 0.03f;
        if (APawn* InstigatorPawn = GetInstigator())
        {
            if (APlayerState* PS = InstigatorPawn->GetPlayerState())
            {
                PingSeconds = FMath::Clamp(PS->GetPingInMilliseconds() * 0.0005f, 0.01f, 0.15f);
            }
        }

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

void AGrenade::OnProjectileStopped(const FHitResult& ImpactResult)
{
    if (!HasAuthority() || bHasExploded) return;

    SettledLocation = GetActorLocation();
    MARK_PROPERTY_DIRTY_FROM_NAME(AGrenade, SettledLocation, this);

    SetNetUpdateFrequency(2.0f);

    if (ProjectileMovementComp)
    {
        ProjectileMovementComp->StopMovementImmediately();
        ProjectileMovementComp->Deactivate();
    }
}

void AGrenade::OnRep_SettledLocation()
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

float AGrenade::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (!HasAuthority() || bHasExploded || DamageAmount <= 0.0f) return 0.0f;

    Explode();
    return DamageAmount;
}

void AGrenade::Explode()
{
    if (!HasAuthority() || bHasExploded) return;

    bHasExploded = true;
    ExplosionLocation = GetActorLocation();
    SettledLocation = FVector::ZeroVector;

    MARK_PROPERTY_DIRTY_FROM_NAME(AGrenade, bHasExploded, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AGrenade, ExplosionLocation, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AGrenade, SettledLocation, this);

    SetNetUpdateFrequency(10.0f);
    ForceNetUpdate();
    GetWorldTimerManager().ClearTimer(FuseTimerHandle);

    if (ProjectileMovementComp)
    {
        ProjectileMovementComp->StopMovementImmediately();
        ProjectileMovementComp->Deactivate();
    }

    PlayExplosionFX(ExplosionLocation);

    AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
    TArray<AActor*> IgnoredActors;
    IgnoredActors.Reserve(GS ? GS->PlayerArray.Num() + 1 : 4);
    IgnoredActors.Add(this);

    APawn* InstigatorPawn = GetInstigator();
    const bool bIsThrowerAlive = (InstigatorPawn != nullptr);

    if (GS)
    {
        for (APlayerState* PS : GS->PlayerArray)
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
        &AGrenade::PrepareForPool,
        0.3f,
        false
    );
}

void AGrenade::OnRep_HasExploded()
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

void AGrenade::PlayExplosionFX(const FVector& Location)
{
    if (GetNetMode() == NM_DedicatedServer) return;

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

void AGrenade::ActivateFromPool(const FTransform& SpawnTransform, const FVector& LaunchVelocity, float InFuseDuration, AController* InInstigatorController)
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

    MARK_PROPERTY_DIRTY_FROM_NAME(AGrenade, bHasExploded, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AGrenade, SettledLocation, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AGrenade, LaunchData, this);

    InitVelocity(LaunchVelocity);

    ForceNetUpdate();
    GetWorldTimerManager().SetTimer(FuseTimerHandle, this, &AGrenade::Explode, FuseDuration, false);
}

void AGrenade::PrepareForPool()
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

    if (ProjectileMovementComp)
    {
        ProjectileMovementComp->StopMovementImmediately();
        ProjectileMovementComp->Deactivate();
    }

    GetWorldTimerManager().ClearTimer(FuseTimerHandle);
    GetWorldTimerManager().ClearTimer(ReturnToPoolTimerHandle);

    FlushNetDormancy();
    SetNetDormancy(DORM_DormantAll);

    if (UThrowablePoolSubsystem* Pool = GetWorld()->GetSubsystem<UThrowablePoolSubsystem>())
    {
        Pool->ReturnGrenade(this);
    }
}