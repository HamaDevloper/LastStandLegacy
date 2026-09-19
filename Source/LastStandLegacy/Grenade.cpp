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
    SetReplicateMovement(false);

    SetNetUpdateFrequency(30.f);
    SetMinNetUpdateFrequency(10.0f);
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

    ProjectileMovementComp->InitialSpeed = 0.0f; // ١. سفر کردنی InitialSpeed تا لە BeginPlayدا ئاڕاستەی خۆکار دروست نەکات
    ProjectileMovementComp->MaxSpeed = 10000.0f; // ٢. بەرزکردنەوەی MaxSpeed تا ئاسانکاری بۆ Velocityی بەهێز بکات
    ProjectileMovementComp->bInitialVelocityInLocalSpace = false; // ٣. لاپۆشکردنی فیزیکی لۆکاڵی Socket
    ProjectileMovementComp->Velocity = FVector::ZeroVector;

    ProjectileMovementComp->bRotationFollowsVelocity = true;
    ProjectileMovementComp->bShouldBounce = true;
    ProjectileMovementComp->Bounciness = 0.2f;
    ProjectileMovementComp->Friction = 0.7f;
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
    DOREPLIFETIME_WITH_PARAMS(AGrenade, InitialVelocity, Params);
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

    APawn* ThrowerPawn = GetInstigator();
    TArray<AActor*> IgnoredActors;
    IgnoredActors.Add(this);


    if(ALastStandLegacyGameState* GS = GetWorld()->GetGameState<ALastStandLegacyGameState>())
    {
        for (APlayerState* PS : GS->PlayerArray)
        {
            if (!PS) continue;
            if (APawn* PlayerPawn = PS->GetPawn())
            {
                if (PlayerPawn != ThrowerPawn)
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