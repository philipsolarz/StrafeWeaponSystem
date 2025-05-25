// Fill out your copyright notice in the Description page of Project Settings.


#include "StickyGrenadeLauncher.h"
#include "StickyGrenadeProjectile.h"

AStickyGrenadeLauncher::AStickyGrenadeLauncher()
{
    // Setup via DataAsset
}


TArray<AStickyGrenadeProjectile*> AStickyGrenadeLauncher::GetStuckProjectiles() const
{
    TArray<AStickyGrenadeProjectile*> Result;

    for (AProjectileBase* Projectile : ActiveProjectiles)
    {
        if (AStickyGrenadeProjectile* Sticky = Cast<AStickyGrenadeProjectile>(Projectile))
        {
            if (Sticky->IsStuckToSurface())
            {
                Result.Add(Sticky);
            }
        }
    }

    return Result;
}