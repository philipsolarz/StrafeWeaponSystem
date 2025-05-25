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
    /** Skeletal mesh for the weapon itself */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USkeletalMeshComponent* WeaponMesh;

    /** Socket on the character to attach to (editable per-weapon in BP or DataAsset) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Attachment")
    FName AttachSocketName = "WeaponSocket";

    /** Data that defines ammo, fire rate, etc. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    UWeaponDataAsset* WeaponData;

    /** Current ammo in the clip (replicated) */
    UPROPERTY(ReplicatedUsing = OnRep_CurrentAmmo, BlueprintReadOnly, Category = "Weapon")
    int32 CurrentAmmo = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon")
    bool  bIsEquipped = false;

    FTimerHandle PrimaryFireTimer;
    FTimerHandle SecondaryFireTimer;

    /** Active projectiles fired by this weapon */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon")
    TArray<AProjectileBase*> ActiveProjectiles;

public:
    /* ---------- core Engine overrides ---------- */
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /* ---------- public weapon interface ---------- */
    UFUNCTION(BlueprintCallable, Category = "Weapon") virtual void StartPrimaryFire();
    UFUNCTION(BlueprintCallable, Category = "Weapon") virtual void StopPrimaryFire();
    UFUNCTION(BlueprintCallable, Category = "Weapon") virtual void StartSecondaryFire();
    UFUNCTION(BlueprintCallable, Category = "Weapon") virtual void StopSecondaryFire();

    UFUNCTION(Server, Reliable) void ServerStartPrimaryFire();
    UFUNCTION(Server, Reliable) void ServerStopPrimaryFire();
    UFUNCTION(Server, Reliable) void ServerStartSecondaryFire();
    UFUNCTION(Server, Reliable) void ServerStopSecondaryFire();

    /* ---------- ammo ---------- */
    UFUNCTION(BlueprintCallable, Category = "Weapon") void AddAmmo(int32 Amount);
    UFUNCTION(BlueprintCallable, Category = "Weapon") bool ConsumeAmmo(int32 Amount = 1);
    UFUNCTION(BlueprintPure, Category = "Weapon") int32 GetCurrentAmmo() const { return CurrentAmmo; }
    UFUNCTION(BlueprintPure, Category = "Weapon") int32 GetMaxAmmo()     const;
    UFUNCTION(BlueprintPure, Category = "Weapon") UWeaponDataAsset* GetWeaponData() const { return WeaponData; }

    /* ---------- equipping ---------- */
    UFUNCTION(BlueprintCallable, Category = "Weapon") virtual void Equip(ACharacter* NewOwner);
    UFUNCTION(BlueprintCallable, Category = "Weapon") virtual void Unequip();

    /* ---------- gameplay events ---------- */
    UPROPERTY(BlueprintAssignable, Category = "Weapon") FOnWeaponFired  OnWeaponFired;
    UPROPERTY(BlueprintAssignable, Category = "Weapon") FOnOutOfAmmo    OnOutOfAmmo;
    UPROPERTY(BlueprintAssignable, Category = "Weapon") FOnAmmoChanged  OnAmmoChanged;

protected:
    /* ---------- internal helpers ---------- */
    virtual void PrimaryFireInternal();
    virtual void SecondaryFireInternal();

    UFUNCTION(BlueprintImplementableEvent, Category = "Weapon") void OnPrimaryFire();
    UFUNCTION(BlueprintImplementableEvent, Category = "Weapon") void OnSecondaryFire();

    UFUNCTION(NetMulticast, Unreliable) void MulticastFireEffects();
    UFUNCTION() void OnRep_CurrentAmmo();

    /* projectile bookkeeping */
    void RegisterProjectile(AProjectileBase* Projectile);
    void UnregisterProjectile(AProjectileBase* Projectile);
    UFUNCTION() void OnProjectileDestroyed(AActor* DestroyedActor);

private:
    friend class AProjectileBase;
};
