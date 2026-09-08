#include "MonkeyBomb.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "ZombieDirectorSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"

AMonkeyBomb::AMonkeyBomb()
{
    PrimaryActorTick.bCanEverTick = false;

    bReplicates = true;
    SetReplicateMovement(true);

    SetNetUpdateFrequency(30.0f);
    SetMinNetUpdateFrequency(2.0f);
    NetCullDistanceSquared = FMath::Square(3000.0f);

    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
    CollisionComp->InitSphereRadius(12.0f);
    CollisionComp->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    RootComponent = CollisionComp;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(CollisionComp);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    AttractionAudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("AttractionAudioComp"));
    AttractionAudioComp->SetupAttachment(MeshComp);
    AttractionAudioComp->bAutoActivate = false;

    ProjectileMovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComp"));
    ProjectileMovementComp->UpdatedComponent = CollisionComp;
    ProjectileMovementComp->InitialSpeed = 1200.0f;
    ProjectileMovementComp->MaxSpeed = 1200.0f;
    ProjectileMovementComp->bRotationFollowsVelocity = true;
    ProjectileMovementComp->bShouldBounce = true;
    ProjectileMovementComp->Bounciness = 0.3f;
    ProjectileMovementComp->Friction = 0.6f;
}

void AMonkeyBomb::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Params;
    Params.bIsPushBased = true;

    DOREPLIFETIME_WITH_PARAMS(AMonkeyBomb, bAttractionActivated, Params);
}

void AMonkeyBomb::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority() && ProjectileMovementComp)
    {
        ProjectileMovementComp->OnProjectileStop.AddDynamic(this, &AMonkeyBomb::OnProjectileStopped);
        ProjectileMovementComp->OnProjectileBounce.AddDynamic(this, &AMonkeyBomb::OnBounce);
    }
}

void AMonkeyBomb::OnProjectileStopped(const FHitResult& ImpactResult)
{
    if (!HasAuthority() || bAttractionActivated || bHasExploded) return;

    ActivateAttraction();
}

void AMonkeyBomb::OnBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
    if (!HasAuthority() || bAttractionActivated || bHasExploded) return;

    if (ImpactVelocity.SizeSquared() < 40000.0f)
    {
        ActivateAttraction();
    }
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

    // 3. تۆمارکردن لە Subsystem
    if (UZombieDirectorSubsystem* Director = GetWorld()->GetSubsystem<UZombieDirectorSubsystem>())
    {
        Director->RegisterAttractor(this, AttractionRadius);
    }

    // 4. بانگهێشتکردنی خۆجێیی بۆ سەرڤەر (چونکە OnRep لەسەر سەرڤەر بە ئۆتۆماتیکی لێنادرێت)
    OnRep_AttractionActivated();

    // 5. بەئاگاهێنانەوەی Network، ناردنی State، پاشان چوونە حالتی Dormant
    FlushNetDormancy();
    SetNetDormancy(DORM_DormantAll);

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

    // ڕاگرتنی تایمەر و پاککردنەوە لە Director
    GetWorldTimerManager().ClearTimer(FuseTimerHandle);

    if (UZombieDirectorSubsystem* Director = GetWorld()->GetSubsystem<UZombieDirectorSubsystem>())
    {
        Director->UnregisterAttractor(this);
    }

    // Server-Authoritative Damage
    TArray<AActor*> IgnoredActors;
    IgnoredActors.Add(this);

    UGameplayStatics::ApplyRadialDamage(
        this,
        BaseDamage,
        GetActorLocation(),
        DamageRadius,
        UDamageType::StaticClass(),
        IgnoredActors,
        this,
        GetInstigatorController(),
        true,
        ECC_Visibility
    );

    // ناردنی FX بۆ کڵاینتەکان پێش Destroy
    FlushNetDormancy();
    Multicast_PlayExplosionFX();

    Destroy();
}

void AMonkeyBomb::Multicast_PlayExplosionFX_Implementation()
{
    if (AttractionAudioComp && AttractionAudioComp->IsPlaying())
    {
        AttractionAudioComp->Stop();
    }

    if (ExplosionSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, GetActorLocation());
    }

    if (ExplosionVFX && GetWorld())
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionVFX, GetActorLocation());
    }
}