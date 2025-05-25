// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponDataAsset.h"
#include "GameplayTagContainer.h"
#include "BaseWeapon.generated.h"

USTRUCT()
struct FStatModifierPair
{
    GENERATED_BODY()

    UPROPERTY()
    FGameplayTag Tag;

    UPROPERTY()
    float        Value = 0.f;
};

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

    /** Map of stat modifiers applied to this weapon (e.g., from power-ups) */
    //UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Weapon|Modifiers", Replicated) Should maybe be replicated, keep in mind for eventual GAS refactoring.
    UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Weapon|Modifiers", Replicated)
    TMap<FGameplayTag, float> StatModifierValues;

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
    UFUNCTION(BlueprintPure, Category = "Weapon") UWeaponDataAsset* GetWeaponData() const { return WeaponData; }

    /* ---------- stat getters with modifier support ---------- */
    UFUNCTION(BlueprintPure, Category = "Weapon|Stats")
    virtual float GetPrimaryFireRate() const;

    UFUNCTION(BlueprintPure, Category = "Weapon|Stats")
    virtual float GetSecondaryFireRate() const;

    UFUNCTION(BlueprintPure, Category = "Weapon|Stats")
    virtual int32 GetMaxAmmo() const;

    UFUNCTION(BlueprintPure, Category = "Weapon|Stats")
    virtual float GetWeaponSwitchTime() const;

    UFUNCTION(BlueprintPure, Category = "Weapon|Stats")
    virtual float GetDamageMultiplier() const;

    UFUNCTION(BlueprintPure, Category = "Weapon|Stats")
    virtual int32 GetMaxActiveProjectiles() const;

    /* ---------- stat modifier management ---------- */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Modifiers", meta = (CallInEditor = "true"))
    virtual void AddStatModifier(FGameplayTag ModifierTag, float Value);

    UFUNCTION(BlueprintCallable, Category = "Weapon|Modifiers", meta = (CallInEditor = "true"))
    virtual void RemoveStatModifier(FGameplayTag ModifierTag);

    UFUNCTION(BlueprintCallable, Category = "Weapon|Modifiers", meta = (CallInEditor = "true"))
    virtual void ClearAllStatModifiers();

    UFUNCTION(BlueprintPure, Category = "Weapon|Modifiers")
    virtual float GetStatModifierValue(FGameplayTag ModifierTag) const;

    UFUNCTION(BlueprintPure, Category = "Weapon|Modifiers")
    virtual bool HasStatModifier(FGameplayTag ModifierTag) const;

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

/*
 * BLUEPRINT IMPLEMENTATION GUIDE: Weapon Stat Modifiers
 *
 * 1. Setting up Gameplay Tags:
 *    - Go to Project Settings -> Project -> Gameplay Tags
 *    - Add tags for different modifiers, for example:
 *      - Weapon.Modifier.Damage.Multiplier (multiplicative damage boost)
 *      - Weapon.Modifier.FireRate.Multiplier (multiplicative fire rate boost)
 *      - Weapon.Modifier.MaxAmmo.Additive (additive max ammo increase)
 *      - Weapon.Modifier.ProjectileSpeed.Multiplier
 *
 * 2. Applying Modifiers from Blueprints (e.g., Power-up Pickup):
 *    - On overlap with player, get the player's current weapon
 *    - Call AddStatModifier with the appropriate tag and value
 *    - Example: AddStatModifier("Weapon.Modifier.Damage.Multiplier", 2.0) for double damage
 *
 * 3. Removing Modifiers (e.g., when power-up expires):
 *    - Call RemoveStatModifier with the same tag used when adding
 *    - Or use ClearAllStatModifiers to remove all modifiers at once
 *
 * 4. UI Integration:
 *    - UI widgets should call the getter functions (GetPrimaryFireRate, GetDamageMultiplier, etc.)
 *    - These functions automatically apply active modifiers to base stats
 *    - Bind UI updates to weapon events like OnAmmoChanged
 *
 * 5. Creating Custom Modifiers:
 *    - For weapon-specific modifiers, override the getter functions in child classes
 *    - Check for custom tags and apply special logic as needed
 */