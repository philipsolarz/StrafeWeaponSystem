// Fill out your copyright notice in the Description page of Project Settings.

#include "RocketProjectile.h"
#include "GameFramework/ProjectileMovementComponent.h"

ARocketProjectile::ARocketProjectile()
{
    // Set default rocket parameters
    if (ProjectileMovement)
    {
        ProjectileMovement->InitialSpeed = 3000.0f;
        ProjectileMovement->MaxSpeed = 3000.0f;
        ProjectileMovement->ProjectileGravityScale = 0.0f; // Rockets fly straight
        ProjectileMovement->bRotationFollowsVelocity = true;
    }

    // Default explosion parameters for rocket
    ExplosionParams.BaseDamage = 100.0f;
    ExplosionParams.MinimumDamage = 10.0f;
    ExplosionParams.InnerRadius = 100.0f;
    ExplosionParams.OuterRadius = 500.0f;
    ExplosionParams.DamageFalloff = 1.0f;
    ExplosionParams.ImpulseStrength = 1000.0f;
    ExplosionParams.SelfDamageMultiplier = 0.5f;
    ExplosionParams.SelfImpulseMultiplier = 1.0f;
    ExplosionParams.bIgnoreOwner = false;

    MaxLifetime = 10.0f;
    bExplodeOnImpact = true;
}

void ARocketProjectile::BeginPlay()
{
    Super::BeginPlay();

    // Any rocket-specific initialization
}

void ARocketProjectile::OnProjectileImpact(const FHitResult& Hit)
{
    // Call parent implementation
    Super::OnProjectileImpact(Hit);

    // Add any rocket-specific impact behavior here
    // For example, you might want to add special effects or sounds
}