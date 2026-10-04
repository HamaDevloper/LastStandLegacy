#include "Grenade.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "LastStandLegacyGameState.h"
#include "GameFramework/PlayerState.h"

AGrenade::AGrenade()
{
    PrimaryActorTick.bCanEverTick = false;

    bReplicates = true;
    SetReplicateMovement(false);

    SetNetUpdateFrequency(10.0f);
    SetMinNetUpdateFrequency(2.0f);
    SetNetCullDistanceSquared(FMath::Square(3500.0f));

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

    ProjectileMovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComp"));
    ProjectileMovementComp->UpdatedComponent = RootComponent;
    ProjectileMovementComp->InitialSpeed = 0.0f;
    ProjectileMovementComp->MaxSpeed = 10000.0f;
    ProjectileMovementComp->bInitialVelocityInLocalSpace = false;
    ProjectileMovementComp->Velocity = FVector::ZeroVector;
    ProjectileMovementComp->bRotationFollowsVelocity = true;
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

    DOREPLIFETIME_WITH_PARAMS_FAST(AGrenade, bHasExploded, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AGrenade, ExplosionLocation, Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(AGrenade, InitialVelocity, Params);
}

void AGrenade::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        GetWorldTimerManager().SetTimer(FuseTimerHandle, this, &AGrenade::Explode, FuseDuration, false);
    }
}

void AGrenade::SetFuseDuration(float NewDuration)
{
    FuseDuration = FMath::Max(0.1f, NewDuration);
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

    MARK_PROPERTY_DIRTY_FROM_NAME(AGrenade, bHasExploded, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(AGrenade, ExplosionLocation, this);

    ForceNetUpdate();

    GetWorldTimerManager().ClearTimer(FuseTimerHandle);

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

void AGrenade::OnRep_HasExploded()
{
    if (bHasExploded)
    {
        PlayExplosionFX(ExplosionLocation);
        SetActorHiddenInGame(true);
        SetActorEnableCollision(false);
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