// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GA_WeaponActivate.h"
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
class STRAFEWEAPONSYSTEM_API UGA_ChargedShotgun_PrimaryFire : public UGA_WeaponActivate
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
    UFUNCTION() // No longer virtual
        void InputReleased(float TimeHeld);

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

    FTimerHandle ChargeTimerHandle;

    bool bIsCharging;
    bool bChargeComplete;
    bool bInputReleasedEarly;

    // Internal functions
    void StartCharge();
    void HandleFullCharge();
    void PerformShot();
    void ResetChargeState();
    void ApplyEarlyReleaseCooldown();
    void ApplyPrimaryFireCooldown();

    UFUNCTION()
    void OnMontageCompleted();

    UFUNCTION()
    void OnMontageBlendOut();

    UFUNCTION()
    void OnMontageInterrupted();

    UFUNCTION()
    void OnMontageCancelled();


    /** Gameplay Tags related to this ability */
    FGameplayTag ChargeInProgressTag;
    FGameplayTag CooldownTagPrimaryFire;

    /** Gameplay Effects */
    TSubclassOf<UGameplayEffect> PrimaryChargeGEClass;
    TSubclassOf<UGameplayEffect> EarlyReleaseCooldownGEClass;
    TSubclassOf<UGameplayEffect> AmmoCostGEClass;

    UPROPERTY(EditDefaultsOnly, Category = "Cooldown")
    TSubclassOf<UGameplayEffect> PrimaryFireCooldownGEClass;

    // How-to extend in Blueprints:
    // - Override K2_ActivateAbility for Blueprint-specific activation logic.
    // - Override K2_CanActivateAbility for Blueprint-specific activation checks.
    // - Create Blueprint-Callable functions for any complex Blueprint interactions needed during the ability.
    // - Use GameplayCue events (via tags in WeaponDataAsset) for visual/audio feedback, triggered from C++ or BP.
    // - Set the PrimaryFireCooldownGEClass in the Blueprint version of this ability.
};