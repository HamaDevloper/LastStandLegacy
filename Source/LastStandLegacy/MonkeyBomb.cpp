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
    SetNetCullDistanceSquared(FMath::Square(3000.0f));

    bAttractionActivated = false;
    bHasExploded = false;

    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
    CollisionComp->InitSphereRadius(12.0f);
    CollisionComp->SetCollisionProfileName(TEXT("BlockAllDynamic"));

    // 🛑 1. بە هیچ شێوەیەک لە کاتی فڕێداندا لەگەڵ یاریزاندا کێشە دروست ناکات
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
    ProjectileMovementComp->InitialSpeed = 2000.0f;
    // 🛑 2. زیاتکردنی MaxSpeed بۆ ئەوەی ڕێگە بدات بە خێرایی بەرز بڕوات
    ProjectileMovementComp->MaxSpeed = 10000.0f;
    ProjectileMovementComp->bRotationFollowsVelocity = true;
    ProjectileMovementComp->bShouldBounce = true;
    ProjectileMovementComp->Bounciness = 0.2f;
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

    GEngine->AddOnScreenDebugMessage(1, 2.f, FColor::Red, "ProjectileStopped");
    ActivateAttraction();
}

void AMonkeyBomb::OnBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
    if (!HasAuthority() || bAttractionActivated || bHasExploded) return;

    GEngine->AddOnScreenDebugMessage(1, 2.f, FColor::Red, "ProjectileBounced");
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

    GEngine->AddOnScreenDebugMessage(1, 2.f, FColor::Red, "bAttractionActivated");
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
    GetWorldTimerManager().ClearTimer(FuseTimerHandle);

    if (UZombieDirectorSubsystem* Director = GetWorld()->GetSubsystem<UZombieDirectorSubsystem>())
    {
        Director->UnregisterAttractor(this);
    }

    // 🟢 1. ڕاسمکردنی بازنەی سووری تەقینەوەکە (Debug Sphere) بۆ ماوەی 5 چڕکە
    DrawDebugSphere(GetWorld(), GetActorLocation(), DamageRadius, 16, FColor::Red, false, 5.0f, 0, 2.0f);

    TArray<AActor*> IgnoredActors;
    IgnoredActors.Add(this);

    // 🛑 2. دۆزینەوەی هەموو یاریزانەکان و زیاکردنیان بۆ لیستی IgnoredActors
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

    // 🔍 3. دۆزینەوەی ئەو ئاکتەرانەی (زۆمبییەکان) کە لە مەودای تەقینەوەکەدان و پرینتکردنی ناویان
    TArray<AActor*> OverlappedActors;
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));

    UKismetSystemLibrary::SphereOverlapActors(
        this,
        GetActorLocation(),
        DamageRadius,
        ObjectTypes,
        AActor::StaticClass(),
        IgnoredActors,
        OverlappedActors
    );

    if (OverlappedActors.Num() > 0)
    {
        for (AActor* HitActor : OverlappedActors)
        {
            if (HitActor)
            {
                if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Orange, FString::Printf(TEXT("[MonkeyBomb Damage] Hit Target: %s"), *HitActor->GetName()));
            }
        }
    }
    else
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("[MonkeyBomb] No targets inside DamageRadius!"));
    }

    // 💣 4. لێدانی Radial Damage (تەنها بەسەر زۆمبی/ئۆبجێکتەکاندا جێبەجێ دەبێت)
    bool bAppliedDamage = UGameplayStatics::ApplyRadialDamage(
        this,
        BaseDamage,
        GetActorLocation(),
        DamageRadius,
        UDamageType::StaticClass(),
        IgnoredActors,
        this,
        GetInstigatorController(),
        true,
        ECC_WorldStatic
    );

    if (GEngine)
    {
        FColor MsgColor = bAppliedDamage ? FColor::Green : FColor::Red;
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, MsgColor, FString::Printf(TEXT("[MonkeyBomb] ApplyRadialDamage Executed: %s | BaseDamage: %.1f"), bAppliedDamage ? TEXT("TRUE") : TEXT("FALSE"), BaseDamage));
    }

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