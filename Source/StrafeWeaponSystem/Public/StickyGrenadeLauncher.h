// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseWeapon.h"
#include "StickyGrenadeLauncher.generated.h"

/**
 * 
 */
UCLASS()
class STRAFEWEAPONSYSTEM_API AStickyGrenadeLauncher : public ABaseWeapon
{
	GENERATED_BODY()

public:
    AStickyGrenadeLauncher();

protected:
    virtual void SecondaryFireInternal() override;

    void DetonateNextSticky();
    TArray<class AStickyGrenadeProjectile*> GetStuckProjectiles() const;
	
};
