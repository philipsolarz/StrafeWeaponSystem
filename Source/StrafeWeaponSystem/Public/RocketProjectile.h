// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ProjectileBase.h"
#include "RocketProjectile.generated.h"

/**
 * Rocket projectile class with specific rocket behavior
 */
UCLASS()
class STRAFEWEAPONSYSTEM_API ARocketProjectile : public AProjectileBase
{
    GENERATED_BODY()

public:
    ARocketProjectile();

protected:
    virtual void BeginPlay() override;

    // Override to add rocket-specific behavior if needed
    virtual void OnProjectileImpact(const FHitResult& Hit);
};