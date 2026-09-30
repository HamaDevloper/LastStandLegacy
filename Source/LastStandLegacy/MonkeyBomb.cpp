#include "MonkeyBomb.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "ZombieDirectorSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Engine/OverlapResult.h"
#include "LastStandLegacyGameState.h"
#include "GameFramework/PlayerState.h"

AMonkeyBomb::AMonkeyBomb()
{
    PrimaryActorTick.bCanEverTick = false;

    bReplicates = true;
    SetReplicateMovement(false);

    SetNetUpdateFrequency(10.f);
    SetMinNetUpdateFrequency(2.0f);
    SetNetCullDistanceSquared(FMath::Square(3500.0f));

    bAttractionActivated = false;
    bHasExploded = false;

    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
    CollisionComp->InitSphereRadius(12.0f);
    CollisionComp->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

    RootComponent = CollisionComp;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(CollisionComp);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    AttractionAudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("AttractionAudioComp"));
    AttractionAudioComp->SetupAttachment(MeshComp);
    AttractionAudioComp->bAutoActivate = false;

    ProjectileMovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComp"));
    ProjectileMovementComp->UpdatedComponent = CollisionComp;

    // 🟢 گۆڕانکارییە سەرەکییەکان:
    ProjectileMovementComp->InitialSpeed = 0.0f;
    ProjectileMovementComp->MaxSpeed = 10000.0f;
    ProjectileMovementComp->bInitialVelocityInLocalSpace = false;
    ProjectileMovementComp->Velocity = FVector::ZeroVector;

    ProjectileMovementComp->bRotationFollowsVelocity = true;
    ProjectileMovementComp->bShouldBounce = true;
    ProjectileMovementComp->Bounciness = 0.15f;
    ProjectileMovementComp->Friction = 0.7f;
}

void AMonkeyBomb::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Params;
    Params.bIsPushBased = true;

    DOREPLIFETIME_WITH_PARAMS_FAST(AMonkeyBomb, bAttractionActivated, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AMonkeyBomb, InitialVelocity, Params);
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

void AMonkeyBomb::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority() && ProjectileMovementComp)
    {
        ProjectileMovementComp->OnProjectileStop.AddDynamic(this, &AMonkeyBomb::OnProjectileStopped);
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

    CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

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

void AMonkeyBomb::Explode()
{
    if (!HasAuthority() || bHasExploded) return;

    bHasExploded = true;
    GetWorldTimerManager().ClearTimer(FuseTimerHandle);

    const FVector ServerExplosionLocation = GetActorLocation();

    if (UZombieDirectorSubsystem* Director = GetWorld()->GetSubsystem<UZombieDirectorSubsystem>())
    {
        Director->UnregisterAttractor(this);
    }

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
        ServerExplosionLocation,
        DamageRadius,
        UDamageType::StaticClass(),
        IgnoredActors,
        this,
        GetInstigatorController(),
        true,
        ECC_WorldStatic
    );


    Multicast_PlayExplosionFX(ServerExplosionLocation);

    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
    SetLifeSpan(0.2f);
}

void AMonkeyBomb::Multicast_PlayExplosionFX_Implementation(FVector_NetQuantize ExplosionLocation)
{
    if (AttractionAudioComp && AttractionAudioComp->IsPlaying())
    {
        AttractionAudioComp->Stop();
    }

    if (ExplosionSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, ExplosionLocation);
    }

    if (ExplosionVFX && GetWorld())
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionVFX, ExplosionLocation);
    }
}