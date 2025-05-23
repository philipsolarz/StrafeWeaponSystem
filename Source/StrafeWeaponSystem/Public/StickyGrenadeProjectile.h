// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ProjectileBase.h"
#include "StickyGrenadeProjectile.generated.h"

/**
 * 
 */
UCLASS()
class STRAFEWEAPONSYSTEM_API AStickyGrenadeProjectile : public AProjectileBase
{
	GENERATED_BODY()
	
public:
    AStickyGrenadeProjectile();

    UFUNCTION(BlueprintPure, Category = "Projectile")
    bool IsStuckToSurface() const { return bIsStuck; }

protected:
    UPROPERTY(ReplicatedUsing = OnRep_IsStuck)
    bool bIsStuck;

    virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, FVector NormalImpulse,
        const FHitResult& Hit) override;

    UFUNCTION()
    void OnRep_IsStuck();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
