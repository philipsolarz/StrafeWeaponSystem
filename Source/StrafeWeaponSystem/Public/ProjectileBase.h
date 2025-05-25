// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProjectileBase.generated.h"


USTRUCT(BlueprintType)
struct FExplosionParams
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    float BaseDamage = 100.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    float MinimumDamage = 10.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    float InnerRadius = 100.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    float OuterRadius = 500.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    float DamageFalloff = 1.0f; // 1.0 = linear, 2.0 = quadratic

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    float ImpulseStrength = 1000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    float SelfDamageMultiplier = 0.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    float SelfImpulseMultiplier = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    bool bIgnoreOwner = false;
};

UCLASS(Abstract)
class STRAFEWEAPONSYSTEM_API AProjectileBase : public AActor
{
    GENERATED_BODY()

public:
    AProjectileBase();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class USphereComponent* CollisionComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UProjectileMovementComponent* ProjectileMovement;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* ProjectileMesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
    FExplosionParams ExplosionParams;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
    float MaxLifetime = 10.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
    bool bExplodeOnImpact = true;

    UPROPERTY(Replicated)
    AController* ProjectileOwner;

    UPROPERTY(Replicated)
    ABaseWeapon* OwningWeapon;

    UPROPERTY()
    TObjectPtr<const UWeaponDataAsset> OwningWeaponData;

    FTimerHandle LifetimeTimer;

public:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintCallable, Category = "Projectile")
    void InitializeProjectile(AController* NewOwner, ABaseWeapon* Weapon, const UWeaponDataAsset* InWeaponData);

    UFUNCTION(BlueprintCallable, Category = "Projectile")
    virtual void Detonate();

    UFUNCTION(Server, Reliable)
    void ServerDetonate();

protected:
    UFUNCTION()
    virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, FVector NormalImpulse,
        const FHitResult& Hit);

    UFUNCTION(BlueprintImplementableEvent, Category = "Projectile")
    void OnProjectileSpawned();

    UFUNCTION(BlueprintImplementableEvent, Category = "Projectile")
    void OnProjectileImpact(const FHitResult& Hit);

    UFUNCTION(BlueprintImplementableEvent, Category = "Projectile")
    void OnExplosion(FVector Location);

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastExplosionEffects(FVector Location);

    void ApplyExplosionDamageAndImpulse(FVector ExplosionLocation);
    void SelfDestruct();
};