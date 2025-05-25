#include "GA_WeaponActivate.h"
#include "StrafeCharacter.h" // For GetStrafeCharacterFromActorInfo
#include "BaseWeapon.h"      // For GetEquippedWeaponFromActorInfo

UGA_WeaponActivate::UGA_WeaponActivate()
{
	// Default settings for weapon abilities
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor; // Or NonInstanced if you manage state carefully
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted; // Good for responsive firing
}

ABaseWeapon* UGA_WeaponActivate::GetEquippedWeaponFromActorInfo() const
{
	AStrafeCharacter* Character = GetStrafeCharacterFromActorInfo();
	if (Character)
	{
		return Character->GetCurrentWeapon();
	}
	return nullptr;
}

AStrafeCharacter* UGA_WeaponActivate::GetStrafeCharacterFromActorInfo() const
{
	return Cast<AStrafeCharacter>(GetAvatarActorFromActorInfo());
}