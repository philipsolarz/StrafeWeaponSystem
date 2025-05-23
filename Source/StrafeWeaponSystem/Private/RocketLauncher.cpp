#include "RocketLauncher.h"
#include "ProjectileBase.h"

ARocketLauncher::ARocketLauncher()
{
    // Rockets will be set up via WeaponDataAsset
}

void ARocketLauncher::SecondaryFireInternal()
{
    if (ActiveProjectiles.Num() == 0)
        return;

    // Find oldest rocket
    AProjectileBase* OldestRocket = nullptr;
    float OldestTime = FLT_MAX;

    for (AProjectileBase* Projectile : ActiveProjectiles)
    {
        if (Projectile && Projectile->GetGameTimeSinceCreation() < OldestTime)
        {
            OldestTime = Projectile->GetGameTimeSinceCreation();
            OldestRocket = Projectile;
        }
    }

    if (OldestRocket)
    {
        OldestRocket->Detonate();
    }

    OnSecondaryFire();
}