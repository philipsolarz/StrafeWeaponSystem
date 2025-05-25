#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_WeaponActivate.generated.h"

class ABaseWeapon;
class AStrafeCharacter;

UCLASS(Abstract) // Abstract as it's meant to be inherited from
class STRAFEWEAPONSYSTEM_API UGA_WeaponActivate : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_WeaponActivate();

	UPROPERTY(EditDefaultsOnly, Category = "Ability") // <<<<<<< ADDED AbilityInputID
	int32 AbilityInputID = -1; // Default to an invalid/unused ID

	/** Retrieves the weapon from the owning character */
	UFUNCTION(BlueprintCallable, Category = "Ability|Weapon")
	ABaseWeapon* GetEquippedWeaponFromActorInfo() const;

	/** Retrieves the StrafeCharacter from the owning actor info */
	UFUNCTION(BlueprintCallable, Category = "Ability|Weapon")
	AStrafeCharacter* GetStrafeCharacterFromActorInfo() const;
};