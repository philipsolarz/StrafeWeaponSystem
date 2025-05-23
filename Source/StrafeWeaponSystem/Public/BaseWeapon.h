// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponDataAsset.h"
#include "BaseWeapon.generated.h"

class AProjectileBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponFired);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnOutOfAmmo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAmmoChanged, int32, NewAmmoCount);

UCLASS(Abstract)
class STRAFEWEAPONSYSTEM_API ABaseWeapon : public AActor
{
    GENERATED_BODY()

public:
    ABaseWeapon();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USkeletalMeshComponent* WeaponMesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    UWeaponDataAsset* WeaponData;

    UPROPERTY(ReplicatedUsing = OnRep_CurrentAmmo, BlueprintReadOnly, Category = "Weapon")
    int32 CurrentAmmo;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon")
    bool bIsEquipped;

    FTimerHandle PrimaryFireTimer;
    FTimerHandle SecondaryFireTimer;

    UPROPERTY(BlueprintReadOnly, Category = "Weapon")
    TArray<AProjectileBase*> ActiveProjectiles;

public:
    // Core Functions
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Weapon Actions
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    virtual void StartPrimaryFire();

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    virtual void StopPrimaryFire();

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    virtual void StartSecondaryFire();

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    virtual void StopSecondaryFire();

    UFUNCTION(Server, Reliable)
    void ServerStartPrimaryFire();

    UFUNCTION(Server, Reliable)
    void ServerStopPrimaryFire();

    UFUNCTION(Server, Reliable)
    void ServerStartSecondaryFire();

    UFUNCTION(Server, Reliable)
    void ServerStopSecondaryFire();

    // Ammo Management
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void AddAmmo(int32 Amount);

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    bool ConsumeAmmo(int32 Amount = 1);

    UFUNCTION(BlueprintPure, Category = "Weapon")
    int32 GetCurrentAmmo() const { return CurrentAmmo; }

    UFUNCTION(BlueprintPure, Category = "Weapon")
    int32 GetMaxAmmo() const;

    UFUNCTION(BlueprintPure, Category = "Weapon")
    UWeaponDataAsset* GetWeaponData() const { return WeaponData; }

    // Equipment
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    virtual void Equip(ACharacter* NewOwner);

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    virtual void Unequip();

    // Events
    UPROPERTY(BlueprintAssignable, Category = "Weapon")
    FOnWeaponFired OnWeaponFired;

    UPROPERTY(BlueprintAssignable, Category = "Weapon")
    FOnOutOfAmmo OnOutOfAmmo;

    UPROPERTY(BlueprintAssignable, Category = "Weapon")
    FOnAmmoChanged OnAmmoChanged;

protected:
    // Internal Fire Logic
    virtual void PrimaryFireInternal();
    virtual void SecondaryFireInternal();

    UFUNCTION(BlueprintImplementableEvent, Category = "Weapon")
    void OnPrimaryFire();

    UFUNCTION(BlueprintImplementableEvent, Category = "Weapon")
    void OnSecondaryFire();

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastFireEffects();

    UFUNCTION()
    void OnRep_CurrentAmmo();

    // Projectile Management
    void RegisterProjectile(AProjectileBase* Projectile);
    void UnregisterProjectile(AProjectileBase* Projectile);

    friend class AProjectileBase;
};