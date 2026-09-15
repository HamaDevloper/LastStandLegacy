#include "Grenade.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Engine/OverlapResult.h"

AGrenade::AGrenade()
{
    PrimaryActorTick.bCanEverTick = false;

    bReplicates = true;
    SetReplicateMovement(false);

    SetNetUpdateFrequency(15.f);
    SetMinNetUpdateFrequency(2.f);
    SetNetCullDistanceSquared(FMath::Square(3000.0f));

    bHasExploded = false;

    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
    CollisionComp->InitSphereRadius(10.0f);
    CollisionComp->SetCollisionProfileName(TEXT("BlockAllDynamic"));

    CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    RootComponent = CollisionComp;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(CollisionComp);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ProjectileMovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComp"));
    ProjectileMovementComp->UpdatedComponent = CollisionComp;
    ProjectileMovementComp->InitialSpeed = 1800.0f;
    ProjectileMovementComp->MaxSpeed = 8000.0f;
    ProjectileMovementComp->bRotationFollowsVelocity = true;
    ProjectileMovementComp->bShouldBounce = true;
    ProjectileMovementComp->Bounciness = 0.3f;
    ProjectileMovementComp->Friction = 0.5f;
}

void AGrenade::SetFuseDuration(float NewDuration)
{
    FuseDuration = FMath::Max(0.05f, NewDuration);
}

void AGrenade::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        GetWorldTimerManager().SetTimer(FuseTimerHandle, this, &AGrenade::Explode, FuseDuration, false);
    }
}

void AGrenade::Explode()
{
    if (!HasAuthority() || bHasExploded) return;

    bHasExploded = true;
    GetWorldTimerManager().ClearTimer(FuseTimerHandle);

    DrawDebugSphere(GetWorld(), GetActorLocation(), DamageRadius, 16, FColor::Red, false, 3.0f, 0, 1.5f);

    TArray<AActor*> IgnoredActors;
    IgnoredActors.Add(this);

    APawn* ThrowerPawn = GetInstigator();

    if (ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>())
    {
        for (APlayerState* PS : GS->PlayerArray)
        {
            if (PS && PS->GetPawn() && PS->GetPawn() != ThrowerPawn)
            {
                IgnoredActors.Add(PS->GetPawn());
            }
        }
    }

    TArray<FOverlapResult> HitResults;
    FCollisionShape Sphere = FCollisionShape::MakeSphere(DamageRadius);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActors(IgnoredActors);

    GetWorld()->OverlapMultiByChannel(
        HitResults,
        GetActorLocation(),
        FQuat::Identity,
        ECC_Bullet,
        Sphere,
        QueryParams
    );

    int32 DamagedZombieCount = 0;
    float TotalDamageDealt = 0.0f;

    for (const FOverlapResult& Result : HitResults)
    {
        if (AActor* HitActor = Result.GetActor())
        {
            DamagedZombieCount++;
            float Distance = FVector::Distance(GetActorLocation(), HitActor->GetActorLocation());
            float DamagePercent = FMath::Clamp(1.0f - (Distance / DamageRadius), 0.0f, 1.0f);
            TotalDamageDealt += BaseDamage * DamagePercent;
        }
    }

    UGameplayStatics::ApplyRadialDamage(
        this,
        BaseDamage,
        GetActorLocation(),
        DamageRadius,
        UDamageType::StaticClass(),
        IgnoredActors,
        this,
        GetInstigatorController(),
        false,
        ECC_WorldStatic
    );

#if !UE_BUILD_SHIPPING
    if (GEngine)
    {
        FString Message = FString::Printf(TEXT("Grenade Exploded! Hit: %d Zombies | Total Damage: %.1f"), DamagedZombieCount, TotalDamageDealt);
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, Message);
    }
#endif

    Multicast_PlayExplosionFX();

    Destroy();
}

void AGrenade::Multicast_PlayExplosionFX_Implementation()
{
    if (ExplosionSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, GetActorLocation());
    }

    if (ExplosionVFX && GetWorld())
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionVFX, GetActorLocation());
    }
}