#include "MonkeyBomb.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "ZombieDirectorSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "LastStandLegacyGameState.h"
#include "GameFramework/PlayerState.h"

AMonkeyBomb::AMonkeyBomb()
{
    PrimaryActorTick.bCanEverTick = false;

    bReplicates = true;
    SetReplicateMovement(false);

    SetNetUpdateFrequency(10.0f);
    SetMinNetUpdateFrequency(2.0f);
    SetNetCullDistanceSquared(FMath::Square(3500.0f));

    bAttractionActivated = false;
    bHasExploded = false;

    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
    CollisionComp->InitSphereRadius(8.0f);
    CollisionComp->SetCollisionProfileName(TEXT("Projectile"));
    CollisionComp->SetCanEverAffectNavigation(false);
    CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
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
    ProjectileMovementComp->Velocity = FVector::ZeroVector;
    ProjectileMovementComp->bRotationFollowsVelocity = true;
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

    DOREPLIFETIME_WITH_PARAMS_FAST(AMonkeyBomb, bAttractionActivated, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AMonkeyBomb, bHasExploded, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AMonkeyBomb, ExplosionLocation, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AMonkeyBomb, InitialVelocity, Params);
}

void AMonkeyBomb::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority() && ProjectileMovementComp)
    {
        ProjectileMovementComp->OnProjectileStop.AddDynamic(this, &AMonkeyBomb::OnProjectileStopped);
    }
}

void AMonkeyBomb::InitVelocity(const FVector& InVelocity)
{
    if (HasAuthority())
    {
        InitialVelocity = InVelocity;
        MARK_PROPERTY_DIRTY_FROM_NAME(AMonkeyBomb, InitialVelocity, this);

        if (ProjectileMovementComp)
        {
            ProjectileMovementComp->Velocity = InVelocity;
        }
    }
}

void AMonkeyBomb::OnRep_InitialVelocity()
{
    if (ProjectileMovementComp)
    {
        ProjectileMovementComp->Velocity = InitialVelocity;
    }
}

void AMonkeyBomb::OnProjectileStopped(const FHitResult& ImpactResult)
{
    if (!HasAuthority() || bAttractionActivated || bHasExploded) return;
    ActivateAttraction();
}

void AMonkeyBomb::ActivateAttraction()
{
    if (!HasAuthority() || bAttractionActivated || bHasExploded) return;

    bAttractionActivated = true;
    MARK_PROPERTY_DIRTY_FROM_NAME(AMonkeyBomb, bAttractionActivated, this);

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

    GetWorldTimerManager().SetTimer(FuseTimerHandle, this, &AMonkeyBomb::Explode, FuseDuration, false);
}

void AMonkeyBomb::OnRep_AttractionActivated()
{
    if (bAttractionActivated && AttractionSound && AttractionAudioComp)
    {
        if (!AttractionAudioComp->IsPlaying())
        {
            AttractionAudioComp->SetSound(AttractionSound);
            AttractionAudioComp->Play();
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
    ExplosionLocation = GetActorLocation();

    MARK_PROPERTY_DIRTY_FROM_NAME(AMonkeyBomb, bHasExploded, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AMonkeyBomb, ExplosionLocation, this);

    ForceNetUpdate();

    GetWorldTimerManager().ClearTimer(FuseTimerHandle);

    if (UZombieDirectorSubsystem* Director = GetWorld()->GetSubsystem<UZombieDirectorSubsystem>())
    {
        Director->UnregisterAttractor(this);
    }

    PlayExplosionFX(ExplosionLocation);

    ALastStandLegacyGameState* GameState = GetWorld()->GetGameState<ALastStandLegacyGameState>();

    TArray<AActor*> IgnoredActors;
    IgnoredActors.Reserve(GameState ? GameState->PlayerArray.Num() + 1 : 3);
    IgnoredActors.Add(this);

    APawn* InstigatorPawn = GetInstigator();

    if (GameState)
    {
        for (APlayerState* PS : GameState->PlayerArray)
        {
            if (!PS) continue;
            if (APawn* PlayerPawn = PS->GetPawn())
            {
                if (PlayerPawn != InstigatorPawn)
                {
                    IgnoredActors.Add(PlayerPawn);
                }
            }
        }
    }

    UGameplayStatics::ApplyRadialDamage(
        this,
        BaseDamage,
        ExplosionLocation,
        DamageRadius,
        UDamageType::StaticClass(),
        IgnoredActors,
        this,
        GetInstigatorController(),
        true,
        ECC_WorldStatic
    );

    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);

    SetLifeSpan(2.0f);
}

void AMonkeyBomb::OnRep_HasExploded()
{
    if (bHasExploded)
    {
        PlayExplosionFX(ExplosionLocation);
        SetActorHiddenInGame(true);
        SetActorEnableCollision(false);
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