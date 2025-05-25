// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseWeapon.h"
#include "RocketLauncher.generated.h"

/**
 * 
 */
UCLASS()
class STRAFEWEAPONSYSTEM_API ARocketLauncher : public ABaseWeapon
{
	GENERATED_BODY()

public:
    ARocketLauncher();

protected:
    //virtual void SecondaryFireInternal() override;

    void DetonateNextRocket();
};
