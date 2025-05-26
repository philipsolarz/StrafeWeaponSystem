// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "GA_ChargedShotgun_SecondaryFire.generated.h"

class AChargedShotgun;
class UWeaponDataAsset;
class UAbilityTask_WaitInputRelease;
class UAbilityTask_PlayMontageAndWait;

/**
 * Gameplay Ability for the Charged Shotgun's Secondary Fire.
 * Handles holding to charge, storing the overcharged shot indefinitely,
 * firing on input release, and applying a weapon lockout cooldown.
 */
UCLASS(Blueprintable)
class STRAFEWEAPONSYSTEM_API UGA_ChargedShotgun_SecondaryFire : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_ChargedShotgun_SecondaryFire();

    /** Returns true if the ability can be activated right now. Checks costs and cooldowns. */
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

    /** Actually activate the ability, should be called by AbilitySystemComponent. */
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    /** Called when the input button for this ability is released. Crucial for firing the stored shot. */
    virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;

    /** Called when the ability ends. */
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

    /** Called when the ability is explicitly cancelled by an external source. */
    virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;

protected:
    UPROPERTY()
    TObjectPtr<AChargedShotgun> EquippedWeapon;

    UPROPERTY()
    TObjectPtr<const UWeaponDataAsset> WeaponData;

    // Tasks
    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitInputRelease> WaitInputReleaseTask;

    UPROPERTY()
    TObjectPtr<UAbilityTask_PlayMontageAndWait> FireMontageTask;

    FTimerHandle SecondaryChargeTimerHandle;

    bool bIsCharging;
    bool bOverchargedShotStored;
    bool bInputWasReleasedDuringCharge; // To detect if input was let go before full charge, but after activation

    // Internal helper functions
    void StartSecondaryCharge();
    void HandleSecondaryFullCharge(); // Called when SecondaryChargeTime is met
    void AttemptFireOverchargedShot();
    void ApplyWeaponLockoutCooldown();
    void ResetAbilityState(bool bClearTimers = true); // Cleans up GEs, tags, optionally timers

    // Montage Callbacks
    UFUNCTION()
    void OnMontageCompleted();

    UFUNCTION()
    void OnMontageBlendOut();

    UFUNCTION()
    void OnMontageInterrupted();

    UFUNCTION()
    void OnMontageCancelled();

    // Gameplay Tags that will be used (fetched from WeaponDataAsset or defined directly)
    FGameplayTag ChargeSecondaryInProgressTag; // e.g., State.Weapon.Charging.Secondary
    FGameplayTag OverchargedStateTag;        // e.g., State.Weapon.Overcharged
    FGameplayTag WeaponLockoutTag;           // e.g., State.Weapon.Lockout (applied by Lockout GE)

    // Gameplay Effects (fetched from WeaponDataAsset)
    TSubclassOf<UGameplayEffect> SecondaryChargeGEClass;
    TSubclassOf<UGameplayEffect> WeaponLockoutGEClass;
    TSubclassOf<UGameplayEffect> AmmoCostGEClass;

    // How-to extend in Blueprints:
    // - Override K2_ActivateAbility for Blueprint-specific activation logic.
    // - Override K2_CanActivateAbility for Blueprint-specific activation checks.
    // - Create Blueprint-Callable functions for any complex Blueprint interactions needed.
    // - GameplayCues (defined in WeaponDataAsset) will handle most visual/audio feedback.
};