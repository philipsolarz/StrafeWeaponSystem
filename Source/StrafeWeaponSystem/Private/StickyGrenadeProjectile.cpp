// Fill out your copyright notice in the Description page of Project Settings.


#include "StickyGrenadeProjectile.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/ProjectileMovementComponent.h"

AStickyGrenadeProjectile::AStickyGrenadeProjectile()
{
    bIsStuck = false;

    // Sticky grenades are affected by gravity
    if (ProjectileMovement)
    {
        ProjectileMovement->ProjectileGravityScale = 1.0f;
    }
}

void AStickyGrenadeProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AStickyGrenadeProjectile, bIsStuck);
}

void AStickyGrenadeProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, FVector NormalImpulse,
    const FHitResult& Hit)
{
    if (!HasAuthority() || bIsStuck)
        return;

    // Don't explode on impact for sticky grenades
    bExplodeOnImpact = false;

    // Stick to surface
    bIsStuck = true;
    ProjectileMovement->StopMovementImmediately();

    // Attach to hit component
    AttachToComponent(OtherComp, FAttachmentTransformRules::KeepWorldTransform, Hit.BoneName);

    OnRep_IsStuck();
}

void AStickyGrenadeProjectile::OnRep_IsStuck()
{
    if (bIsStuck)
    {
        // Visual feedback for sticking
        OnProjectileImpact(FHitResult());
    }
}