// Fill out your copyright notice in the Description page of Project Settings.


#include "StickyGrenadeProjectile.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "WeaponDataAsset.h"
#include "GameFramework/Character.h"

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

    // Check if we can stick to this actor
    bool bShouldStick = true;

    if (OwningWeaponData)
    {
        // Check if we hit a character and if we're allowed to stick to characters
        if (Cast<ACharacter>(OtherActor) && !OwningWeaponData->WeaponStats.bCanStickToCharacters)
        {
            bShouldStick = false;
        }
    }

    if (bShouldStick)
    {
        // Don't explode on impact for sticky grenades
        bExplodeOnImpact = false;

        // Stick to surface
        bIsStuck = true;
        ProjectileMovement->StopMovementImmediately();

        // Attach with offset if specified
        FVector AttachOffset = FVector::ZeroVector;
        if (OwningWeaponData && OwningWeaponData->WeaponStats.StickyAttachmentOffset > 0.0f)
        {
            AttachOffset = Hit.ImpactNormal * OwningWeaponData->WeaponStats.StickyAttachmentOffset;
        }

        SetActorLocation(GetActorLocation() + AttachOffset);
        AttachToComponent(OtherComp, FAttachmentTransformRules::KeepWorldTransform, Hit.BoneName);

        OnRep_IsStuck();
    }
    else
    {
        // Bounce off if we can't stick
        ProjectileMovement->Velocity = ProjectileMovement->Velocity.MirrorByVector(Hit.ImpactNormal) * 0.6f;
    }
}

void AStickyGrenadeProjectile::OnRep_IsStuck()
{
    if (bIsStuck)
    {
        // Visual feedback for sticking
        OnProjectileImpact(FHitResult());
    }
}