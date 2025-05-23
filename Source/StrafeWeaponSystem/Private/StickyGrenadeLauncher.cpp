// Fill out your copyright notice in the Description page of Project Settings.


#include "StickyGrenadeLauncher.h"
#include "StickyGrenadeProjectile.h"

AStickyGrenadeLauncher::AStickyGrenadeLauncher()
{
    // Setup via DataAsset
}

void AStickyGrenadeLauncher::SecondaryFireInternal()
{
    TArray<AStickyGrenadeProjectile*> StuckProjectiles = GetStuckProjectiles();

    if (StuckProjectiles.Num() == 0)
        return;

    // Find oldest stuck grenade
    AStickyGrenadeProjectile* OldestSticky = nullptr;
    float OldestTime = FLT_MAX;

    for (AStickyGrenadeProjectile* Sticky : StuckProjectiles)
    {
        if (Sticky && Sticky->GetGameTimeSinceCreation() < OldestTime)
        {
            OldestTime = Sticky->GetGameTimeSinceCreation();
            OldestSticky = Sticky;
        }
    }

    if (OldestSticky)
    {
        OldestSticky->Detonate();
    }

    OnSecondaryFire();
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