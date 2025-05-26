// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "GA_ChargedShotgun_PrimaryFire.generated.h"

class AChargedShotgun;
class UWeaponDataAsset;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitInputRelease;
class UAbilityTask_PlayMontageAndWait;

/**
 * Gameplay Ability for the Charged Shotgun's Primary Fire.
 * Handles charging, auto-firing on full charge, early release cancellation,
 * and continuous charging if the input is held.
 */
UCLASS(Blueprintable)
class STRAFEWEAPONSYSTEM_API UGA_ChargedShotgun_PrimaryFire : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_ChargedShotgun_PrimaryFire();

    /** Returns true if the ability can be activated right now. Checks costs and cooldowns. */
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

    /** Actually activate the ability, should be called by AbilitySystemComponent. */
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    /** Called when the ability is explicitly cancelled by an external source. */
    virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;

    /** Called when the ability ends. */
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

    /** Called when the input button for this ability is released. */
    virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;

protected:
    UPROPERTY()
    AChargedShotgun* EquippedWeapon;

    UPROPERTY()
    const UWeaponDataAsset* WeaponData;

    // Tasks
    UPROPERTY()
    UAbilityTask_WaitInputRelease* WaitInputReleaseTask;

    UPROPERTY()
    UAbilityTask_PlayMontageAndWait* FireMontageTask;

    FTimerHandle ChargeTimerHandle;
    FTimerHandle ContinuousChargeDelayTimerHandle; // Timer to delay before continuous charge starts

    bool bIsCharging;
    bool bChargeComplete;
    bool bInputReleasedEarly; // Flag to indicate if input was released before full charge

    // Internal functions
    void StartCharge();
    void HandleFullCharge();
    void PerformShot();
    void ResetChargeState();
    void ApplyEarlyReleaseCooldown();
    void ApplyPrimaryFireCooldown();
    void StartContinuousChargeCheck();
    void AttemptContinuousCharge();


    UFUNCTION()
    void OnMontageCompleted();

    UFUNCTION()
    void OnMontageBlendOut();

    UFUNCTION()
    void OnMontageInterrupted();

    UFUNCTION()
    void OnMontageCancelled();


    /** Gameplay Tags related to this ability */
    FGameplayTag ChargeInProgressTag; // e.g., State.Weapon.Charging.Primary
    FGameplayTag CooldownTagPrimaryFire; // Specific cooldown tag for primary fire from WeaponDataAsset

    /** Gameplay Effects */
    TSubclassOf<UGameplayEffect> PrimaryChargeGEClass;
    TSubclassOf<UGameplayEffect> EarlyReleaseCooldownGEClass;
    TSubclassOf<UGameplayEffect> AmmoCostGEClass;
    TSubclassOf<UGameplayEffect> PrimaryFireCooldownGEClass; // This GE will have the CooldownTagPrimaryFire

    // How-to extend in Blueprints:
    // - Override K2_ActivateAbility for Blueprint-specific activation logic.
    // - Override K2_CanActivateAbility for Blueprint-specific activation checks.
    // - Create Blueprint-Callable functions for any complex Blueprint interactions needed during the ability.
    // - Use GameplayCue events (via tags in WeaponDataAsset) for visual/audio feedback, triggered from C++ or BP.
};