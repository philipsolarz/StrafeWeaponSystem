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
    UFUNCTION() // Added UFUNCTION for delegate binding
        void InputReleased(float TimeHeld); // Changed signature

protected:
    UPROPERTY()
    TObjectPtr<AChargedShotgun> EquippedWeapon; // Corrected to TObjectPtr

    UPROPERTY()
    TObjectPtr<const UWeaponDataAsset> WeaponData; // Corrected to TObjectPtr

    // Tasks
    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitInputRelease> WaitInputReleaseTask; // Corrected to TObjectPtr

    UPROPERTY()
    TObjectPtr<UAbilityTask_PlayMontageAndWait> FireMontageTask; // Corrected to TObjectPtr

    FTimerHandle ChargeTimerHandle;
    // FTimerHandle ContinuousChargeDelayTimerHandle; // Removed for now, relying on bRetriggerInstancedAbility

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
    // void StartContinuousChargeCheck(); // Removed
    // void AttemptContinuousCharge(); // Removed


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

    UPROPERTY(EditDefaultsOnly, Category = "Cooldown") // Exposed to BP to set the Cooldown GE
        TSubclassOf<UGameplayEffect> PrimaryFireCooldownGEClass;

    // How-to extend in Blueprints:
    // - Override K2_ActivateAbility for Blueprint-specific activation logic.
    // - Override K2_CanActivateAbility for Blueprint-specific activation checks.
    // - Create Blueprint-Callable functions for any complex Blueprint interactions needed during the ability.
    // - Use GameplayCue events (via tags in WeaponDataAsset) for visual/audio feedback, triggered from C++ or BP.
    // - Set the PrimaryFireCooldownGEClass in the Blueprint version of this ability.
};