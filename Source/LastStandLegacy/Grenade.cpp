#include "Grenade.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Engine/OverlapResult.h"

AGrenade::AGrenade()
{
    PrimaryActorTick.bCanEverTick = false;

    bReplicates = true;
    SetReplicateMovement(true);

    SetNetUpdateFrequency(30.f);
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
    ProjectileMovementComp->Bounciness = 0.2f;
    ProjectileMovementComp->Friction = 0.6f;
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

    const FVector ServerExplosionLocation = GetActorLocation() + FVector(0.0f, 0.0f, 10.0f);

#if !UE_BUILD_SHIPPING
    DrawDebugSphere(GetWorld(), ServerExplosionLocation, DamageRadius, 16, FColor::Red, false, 3.0f, 0, 1.5f);
#endif

    TArray<AActor*> IgnoredActors;
    IgnoredActors.Add(this);


    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (APlayerController* PC = It->Get())
        {
            if (APawn* PlayerPawn = PC->GetPawn())
            {
                IgnoredActors.Add(PlayerPawn);
            }
        }
    }

    TArray<FOverlapResult> HitResults;
    FCollisionShape Sphere = FCollisionShape::MakeSphere(DamageRadius);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActors(IgnoredActors);

    GetWorld()->OverlapMultiByChannel(
        HitResults,
        ServerExplosionLocation,
        FQuat::Identity,
        ECC_Pawn,
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
            float Distance = FVector::Distance(ServerExplosionLocation, HitActor->GetActorLocation());
            float DamagePercent = FMath::Clamp(1.0f - (Distance / DamageRadius), 0.0f, 1.0f);
            TotalDamageDealt += BaseDamage * DamagePercent;
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

#if !UE_BUILD_SHIPPING
    if (GEngine)
    {
        FString Message = FString::Printf(TEXT("Grenade Exploded! Hit: %d Zombies | Total Damage: %.1f"), DamagedZombieCount, TotalDamageDealt);
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, Message);
    }
#endif

    Multicast_PlayExplosionFX(ServerExplosionLocation);

    Destroy();
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