#include "ProjectileBase.h"
#include "BaseWeapon.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "TimerManager.h"
#include "CollisionQueryParams.h"

AProjectileBase::AProjectileBase()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
    CollisionComp->InitSphereRadius(5.0f);
    CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CollisionComp->SetCollisionResponseToAllChannels(ECR_Block);
    CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    RootComponent = CollisionComp;

    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileComp"));
    ProjectileMovement->UpdatedComponent = CollisionComp;
    ProjectileMovement->InitialSpeed = 3000.f;
    ProjectileMovement->MaxSpeed = 3000.f;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->bShouldBounce = false;
    ProjectileMovement->ProjectileGravityScale = 0.0f;

    ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
    ProjectileMesh->SetupAttachment(CollisionComp);
    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    CollisionComp->OnComponentHit.AddDynamic(this, &AProjectileBase::OnHit);
}

void AProjectileBase::BeginPlay()
{
    Super::BeginPlay();

    OnProjectileSpawned();

    if (MaxLifetime > 0.0f)
    {
        GetWorld()->GetTimerManager().SetTimer(
            LifetimeTimer,
            this,
            &AProjectileBase::SelfDestruct,
            MaxLifetime,
            false
        );
    }
}

void AProjectileBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AProjectileBase, ProjectileOwner);
    DOREPLIFETIME(AProjectileBase, OwningWeapon);
}

void AProjectileBase::InitializeProjectile(AController* NewOwner, ABaseWeapon* Weapon)
{
    ProjectileOwner = NewOwner;
    OwningWeapon = Weapon;

    // Ignore collision with owner
    if (AActor* OwnerActor = NewOwner ? NewOwner->GetPawn() : nullptr)
    {
        CollisionComp->IgnoreActorWhenMoving(OwnerActor, true);
        CollisionComp->MoveIgnoreActors.Add(OwnerActor);
    }
}

void AProjectileBase::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, FVector NormalImpulse,
    const FHitResult& Hit)
{
    if (!HasAuthority())
        return;

    OnProjectileImpact(Hit);

    if (bExplodeOnImpact)
    {
        Detonate();
    }
}

void AProjectileBase::Detonate()
{
    if (!HasAuthority())
    {
        ServerDetonate();
        return;
    }

    FVector ExplosionLocation = GetActorLocation();

    ApplyExplosionDamageAndImpulse(ExplosionLocation);
    MulticastExplosionEffects(ExplosionLocation);

    if (OwningWeapon)
    {
        OwningWeapon->UnregisterProjectile(this);
    }

    Destroy();
}

void AProjectileBase::ServerDetonate_Implementation()
{
    Detonate();
}

void AProjectileBase::ApplyExplosionDamageAndImpulse(FVector ExplosionLocation)
{
    // Debug visualization
#if WITH_EDITOR
    if (GetWorld()->GetNetMode() != NM_DedicatedServer)
    {
        DrawDebugSphere(GetWorld(), ExplosionLocation, ExplosionParams.InnerRadius, 12, FColor::Red, false, 2.0f);
        DrawDebugSphere(GetWorld(), ExplosionLocation, ExplosionParams.OuterRadius, 12, FColor::Orange, false, 2.0f);
    }
#endif

    // Apply radial damage - this handles both damage and impulse
    TArray<AActor*> IgnoredActors;
    if (ExplosionParams.bIgnoreOwner && GetOwner())
    {
        IgnoredActors.Add(GetOwner());
    }

    // Apply radial damage with falloff
    UGameplayStatics::ApplyRadialDamageWithFalloff(
        GetWorld(),
        ExplosionParams.BaseDamage,
        ExplosionParams.MinimumDamage,
        ExplosionLocation,
        ExplosionParams.InnerRadius,
        ExplosionParams.OuterRadius,
        ExplosionParams.DamageFalloff,
        UDamageType::StaticClass(),
        IgnoredActors,
        this,
        ProjectileOwner,
        ECollisionChannel::ECC_Visibility // Damage trace channel
    );

    // Apply additional impulse for better rocket jumping
    // The radial damage doesn't always provide enough impulse for movement
    TArray<FHitResult> HitResults;
    FCollisionShape CollisionShape = FCollisionShape::MakeSphere(ExplosionParams.OuterRadius);

    GetWorld()->SweepMultiByChannel(
        HitResults,
        ExplosionLocation,
        ExplosionLocation,
        FQuat::Identity,
        ECollisionChannel::ECC_Pawn,
        CollisionShape
    );

    for (const FHitResult& Hit : HitResults)
    {
        AActor* HitActor = Hit.GetActor();
        if (!HitActor || (ExplosionParams.bIgnoreOwner && HitActor == GetOwner()))
            continue;

        // Calculate distance and falloff
        float Distance = FVector::Dist(ExplosionLocation, HitActor->GetActorLocation());
        float Alpha = FMath::Clamp((ExplosionParams.OuterRadius - Distance) /
            (ExplosionParams.OuterRadius - ExplosionParams.InnerRadius), 0.0f, 1.0f);

        // Apply falloff curve
        float FalloffMultiplier = FMath::Pow(Alpha, ExplosionParams.DamageFalloff);

        // Check for self damage
        bool bIsSelfDamage = (HitActor == GetInstigator());

        // Apply impulse to characters for rocket jumping
        if (ACharacter* Character = Cast<ACharacter>(HitActor))
        {
            if (UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement())
            {
                FVector LaunchDirection = (Character->GetActorLocation() - ExplosionLocation).GetSafeNormal();
                LaunchDirection.Z = FMath::Abs(LaunchDirection.Z) + 0.3f; // Add upward bias for better jumping
                LaunchDirection.Normalize();

                float LaunchMultiplier = bIsSelfDamage ? ExplosionParams.SelfImpulseMultiplier : 1.0f;
                float LaunchMagnitude = ExplosionParams.ImpulseStrength * FalloffMultiplier * LaunchMultiplier;

                Character->LaunchCharacter(LaunchDirection * LaunchMagnitude, true, true);
            }
        }
        // Apply impulse to physics objects
        else if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(HitActor->GetRootComponent()))
        {
            if (PrimComp->IsSimulatingPhysics())
            {
                FVector ImpulseDirection = (HitActor->GetActorLocation() - ExplosionLocation).GetSafeNormal();
                float ImpulseMagnitude = ExplosionParams.ImpulseStrength * FalloffMultiplier;

                PrimComp->AddImpulseAtLocation(ImpulseDirection * ImpulseMagnitude, ExplosionLocation);
            }
        }
    }
}

void AProjectileBase::MulticastExplosionEffects_Implementation(FVector Location)
{
    OnExplosion(Location);
}

void AProjectileBase::SelfDestruct()
{
    if (HasAuthority())
    {
        if (OwningWeapon)
        {
            OwningWeapon->UnregisterProjectile(this);
        }
        Destroy();
    }
}