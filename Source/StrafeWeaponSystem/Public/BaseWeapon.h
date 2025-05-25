// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponDataAsset.h"
#include "GameplayTagContainer.h"
#include "BaseWeapon.generated.h"

// USTRUCT() // FStatModifierPair is no longer needed with GAS handling most modifiers
// struct FStatModifierPair
// {
//     GENERATED_BODY()
//
//     UPROPERTY()
//     FGameplayTag Tag;
//
//     UPROPERTY()
//     float        Value = 0.f;
// };

class AProjectileBase;
class USkeletalMeshComponent;


//DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponFired); // Will be handled by GameplayCues or ability events
//DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnOutOfAmmo);   // Handled by UI observing attributes or specific "OutOfAmmo" ability
//DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAmmoChanged, int32, NewAmmoCount); // Handled by UI observing attributes

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
    FName AttachSocketName = "WeaponSocket"; // Still relevant for visual attachment

    /** Data that defines ammo, fire rate, etc. Now primarily for ability configuration. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    UWeaponDataAsset* WeaponData;

    // CurrentAmmo, MaxAmmo, StatModifierValues are removed. Handled by Character's AttributeSet & GameplayEffects.

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon")
    bool  bIsEquipped = false; // Still useful to know if the actor is visually equipped

    // FTimerHandle PrimaryFireTimer; // Handled by Ability Cooldowns
    // FTimerHandle SecondaryFireTimer; // Handled by Ability Cooldowns

    /** Active projectiles fired by this weapon - This might still be useful for some weapon logic (e.g. detonating existing projectiles)
     * but the core firing mechanism won't rely on it as much.
     */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon")
    TArray<AProjectileBase*> ActiveProjectiles;


public:
    /* ---------- core Engine overrides ---------- */
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Firing methods are removed. Will be handled by Gameplay Abilities.
    // UFUNCTION(BlueprintCallable, Category = "Weapon") virtual void StartPrimaryFire();
    // UFUNCTION(BlueprintCallable, Category = "Weapon") virtual void StopPrimaryFire();
    // UFUNCTION(BlueprintCallable, Category = "Weapon") virtual void StartSecondaryFire();
    // UFUNCTION(BlueprintCallable, Category = "Weapon") virtual void StopSecondaryFire();
    //
    // UFUNCTION(Server, Reliable) void ServerStartPrimaryFire();
    // UFUNCTION(Server, Reliable) void ServerStopPrimaryFire();
    // UFUNCTION(Server, Reliable) void ServerStartSecondaryFire();
    // UFUNCTION(Server, Reliable) void ServerStopSecondaryFire();

    /* ---------- ammo - These are removed. Ammo is in Character's AttributeSet ---------- */
    // UFUNCTION(BlueprintCallable, Category = "Weapon") void AddAmmo(int32 Amount);
    // UFUNCTION(BlueprintCallable, Category = "Weapon") bool ConsumeAmmo(int32 Amount = 1);
    // UFUNCTION(BlueprintPure, Category = "Weapon") int32 GetCurrentAmmo() const { return CurrentAmmo; }

    UFUNCTION(BlueprintPure, Category = "Weapon")
    UWeaponDataAsset* GetWeaponData() const { return WeaponData; }

    UFUNCTION(BlueprintPure, Category = "Components")
    USkeletalMeshComponent* GetWeaponMeshComponent() const { return WeaponMesh; }


    /* ---------- stat getters with modifier support - These are mostly removed or refactored.
                 Base stats come from WeaponData, modifiers are via GameplayEffects on Character.
                 The ability itself will query these or have them as part of its definition.
     ---------- */
     // UFUNCTION(BlueprintPure, Category = "Weapon|Stats")
     // virtual float GetPrimaryFireRate() const; // Now comes from WeaponData for ability's cooldown GE
     //
     // UFUNCTION(BlueprintPure, Category = "Weapon|Stats")
     // virtual float GetSecondaryFireRate() const; // Now comes from WeaponData for ability's cooldown GE
     //
     // UFUNCTION(BlueprintPure, Category = "Weapon|Stats")
     // virtual int32 GetMaxAmmo() const; // From Character's AttributeSet
     //
     // UFUNCTION(BlueprintPure, Category = "Weapon|Stats")
     // virtual float GetWeaponSwitchTime() const; // Still relevant, could be in WeaponData
     //
     // UFUNCTION(BlueprintPure, Category = "Weapon|Stats")
     // virtual float GetDamageMultiplier() const; // From Character's AttributeSet or Projectile's GE
     //
     // UFUNCTION(BlueprintPure, Category = "Weapon|Stats")
     // virtual int32 GetMaxActiveProjectiles() const; // Could be checked by ability or a gameplay tag on weapon

     /* ---------- stat modifier management - Removed. Use GameplayEffects. ---------- */
     // UFUNCTION(BlueprintCallable, Category = "Weapon|Modifiers", meta = (CallInEditor = "true"))
     // virtual void AddStatModifier(FGameplayTag ModifierTag, float Value);
     //
     // UFUNCTION(BlueprintCallable, Category = "Weapon|Modifiers", meta = (CallInEditor = "true"))
     // virtual void RemoveStatModifier(FGameplayTag ModifierTag);
     //
     // UFUNCTION(BlueprintCallable, Category = "Weapon|Modifiers", meta = (CallInEditor = "true"))
     // virtual void ClearAllStatModifiers();
     //
     // UFUNCTION(BlueprintPure, Category = "Weapon|Modifiers")
     // virtual float GetStatModifierValue(FGameplayTag ModifierTag) const;
     //
     // UFUNCTION(BlueprintPure, Category = "Weapon|Modifiers")
     // virtual bool HasStatModifier(FGameplayTag ModifierTag) const;

     /* ---------- equipping ---------- */
    UFUNCTION(BlueprintCallable, Category = "Weapon") virtual void Equip(ACharacter* NewOwner);
    UFUNCTION(BlueprintCallable, Category = "Weapon") virtual void Unequip();

    bool IsEquipped() const { return bIsEquipped; }


    /* ---------- gameplay events - Removed. Use GameplayCues or Ability events. ---------- */
    // UPROPERTY(BlueprintAssignable, Category = "Weapon") FOnWeaponFired  OnWeaponFired;
    // UPROPERTY(BlueprintAssignable, Category = "Weapon") FOnOutOfAmmo    OnOutOfAmmo;
    // UPROPERTY(BlueprintAssignable, Category = "Weapon") FOnAmmoChanged  OnAmmoChanged;

protected:
    /* ---------- internal helpers - Removed or refactored. ---------- */
    // virtual void PrimaryFireInternal();
    // virtual void SecondaryFireInternal();
    //
    // UFUNCTION(BlueprintImplementableEvent, Category = "Weapon") void OnPrimaryFire();
    // UFUNCTION(BlueprintImplementableEvent, Category = "Weapon") void OnSecondaryFire();
    //
    // UFUNCTION(NetMulticast, Unreliable) void MulticastFireEffects();
    // UFUNCTION() void OnRep_CurrentAmmo();

    /* projectile bookkeeping - Still potentially useful for some weapons (e.g. sticky launcher detonate all) */
public: // Make public if other systems (like the ability itself) need to interact
    void RegisterProjectile(AProjectileBase* Projectile);
    void UnregisterProjectile(AProjectileBase* Projectile);
protected:
    UFUNCTION() void OnProjectileDestroyed(AActor* DestroyedActor);


private:
    friend class AProjectileBase; // Still useful for projectile to access OwningWeapon
};