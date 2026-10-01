#include "Grenade.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Engine/OverlapResult.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "LastStandLegacyGameState.h"
#include "GameFramework/PlayerState.h"

AGrenade::AGrenade()
{
    PrimaryActorTick.bCanEverTick = false;

    bReplicates = true;
    SetReplicateMovement(false); // دروستە: بۆ خۆدوورگرتن لە زۆری باری تۆڕ

    SetNetUpdateFrequency(10.0f);
    SetMinNetUpdateFrequency(2.0f);
    SetNetCullDistanceSquared(FMath::Square(3500.0f));

    bHasExploded = false;

    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
    CollisionComp->InitSphereRadius(6.0f);
    CollisionComp->SetCollisionProfileName(TEXT("Projectile"));

    CollisionComp->SetCanEverAffectNavigation(false);

    CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
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
    ProjectileMovementComp->Velocity = FVector::ZeroVector;

    ProjectileMovementComp->bRotationFollowsVelocity = true;
    ProjectileMovementComp->bShouldBounce = true;
    ProjectileMovementComp->Bounciness = 0.25f;
    ProjectileMovementComp->Friction = 0.6f;

    ProjectileMovementComp->BounceVelocityStopSimulatingThreshold = 5.0f;
}

void AGrenade::SetFuseDuration(float NewDuration)
{
    FuseDuration = FMath::Max(0.05f, NewDuration);
}

void AGrenade::InitVelocity(const FVector& InVelocity)
{
    if (HasAuthority())
    {
        InitialVelocity = InVelocity;
        MARK_PROPERTY_DIRTY_FROM_NAME(AGrenade, InitialVelocity, this);

        if (ProjectileMovementComp)
        {
            ProjectileMovementComp->Velocity = InVelocity;
        }
    }
}

void AGrenade::OnRep_InitialVelocity()
{
    if (ProjectileMovementComp)
    {
        ProjectileMovementComp->Velocity = InitialVelocity;
    }
}

void AGrenade::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        GetWorldTimerManager().SetTimer(FuseTimerHandle, this, &AGrenade::Explode, FuseDuration, false);
    }
}

void AGrenade::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    FDoRepLifetimeParams Params;
    Params.bIsPushBased = true;
    DOREPLIFETIME_WITH_PARAMS_FAST(AGrenade, InitialVelocity, Params);
}

float AGrenade::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (!HasAuthority() || bHasExploded || DamageAmount <= 0) return 0.f;
    Explode();
    return DamageAmount;
}

void AGrenade::Explode()
{
    if (!HasAuthority() || bHasExploded) return;

    bHasExploded = true;
    GetWorldTimerManager().ClearTimer(FuseTimerHandle);

    const FVector ServerExplosionLocation = GetActorLocation() + FVector(0.0f, 0.0f, 10.0f);

    APawn* ThrowerPawn = GetInstigator();

    ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>();

    TArray<AActor*> IgnoredActors;
    IgnoredActors.Reserve(GS? GS->PlayerArray.Num() + 1 : 3);
    IgnoredActors.Add(this);

    if (GS)
    {
        for (APlayerState* PS : GS->PlayerArray)
        {
            if (!PS) continue;
            if (APawn* PlayerPawn = PS->GetPawn())
            {
                if (PlayerPawn != ThrowerPawn)
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
        false,
        ECC_WorldStatic
    );

    Multicast_PlayExplosionFX(ServerExplosionLocation);

    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
    SetLifeSpan(1.f);
}

void AGrenade::Multicast_PlayExplosionFX_Implementation(FVector_NetQuantize ExplosionLocation)
{
    if (ExplosionSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, ExplosionLocation);
    }

    if (ExplosionVFX && GetWorld())
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionVFX, ExplosionLocation);
    }
}