#pragma once

#include "CoreMinimal.h"
#include "GA_WeaponActivate.h"
#include "GA_WeaponFire.generated.h"

class AProjectileBase;
class UWeaponDataAsset;

UCLASS()
class STRAFEWEAPONSYSTEM_API UGA_WeaponFire : public UGA_WeaponActivate
{
	GENERATED_BODY()

public:
	UGA_WeaponFire();

	/** Actually activate the ability, firing the weapon */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** Checks if this ability can be activated */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	/** Called when the ability is committed (cost is paid, cooldown applied) */
	virtual void CommitExecute(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;


protected:
	// Helper function to get WeaponData from the equipped weapon
	const UWeaponDataAsset* GetWeaponData() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|Weapon")
	void K2_OnWeaponFired(); // For Blueprint effects

	// Spawns the projectile. Can be overridden in BP for custom projectile spawning logic if needed.
	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Weapon")
	void SpawnProjectile(ABaseWeapon* Weapon, const FVector& SpawnLocation, const FRotator& SpawnRotation);
	virtual void SpawnProjectile_Implementation(ABaseWeapon* Weapon, const FVector& SpawnLocation, const FRotator& SpawnRotation);
};