#include "StrafeAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h" // Required for FGameplayEffectModCallbackData

UStrafeAttributeSet::UStrafeAttributeSet()
{
	// Initialize default values here if needed, or do it in AStrafeCharacter when initializing attributes
}

void UStrafeAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UStrafeAttributeSet, RocketAmmo, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UStrafeAttributeSet, MaxRocketAmmo, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UStrafeAttributeSet, StickyGrenadeAmmo, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UStrafeAttributeSet, MaxStickyGrenadeAmmo, COND_None, REPNOTIFY_Always);
	// DOREPLIFETIME_CONDITION_NOTIFY(UStrafeAttributeSet, Health, COND_None, REPNOTIFY_Always);
	// DOREPLIFETIME_CONDITION_NOTIFY(UStrafeAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	// DOREPLIFETIME_CONDITION_NOTIFY(UStrafeAttributeSet, MovementSpeed, COND_None, REPNOTIFY_Always);
}

void UStrafeAttributeSet::OnRep_RocketAmmo(const FGameplayAttributeData& OldRocketAmmo)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UStrafeAttributeSet, RocketAmmo, OldRocketAmmo);
}

void UStrafeAttributeSet::OnRep_MaxRocketAmmo(const FGameplayAttributeData& OldMaxRocketAmmo)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UStrafeAttributeSet, MaxRocketAmmo, OldMaxRocketAmmo);
}

void UStrafeAttributeSet::OnRep_StickyGrenadeAmmo(const FGameplayAttributeData& OldStickyGrenadeAmmo)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UStrafeAttributeSet, StickyGrenadeAmmo, OldStickyGrenadeAmmo);
}

void UStrafeAttributeSet::OnRep_MaxStickyGrenadeAmmo(const FGameplayAttributeData& OldMaxStickyGrenadeAmmo)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UStrafeAttributeSet, MaxStickyGrenadeAmmo, OldMaxStickyGrenadeAmmo);
}

// void UStrafeAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
// {
// 	GAMEPLAYATTRIBUTE_REPNOTIFY(UStrafeAttributeSet, Health, OldHealth);
// }
//
// void UStrafeAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
// {
// 	GAMEPLAYATTRIBUTE_REPNOTIFY(UStrafeAttributeSet, MaxHealth, OldMaxHealth);
// }
//
// void UStrafeAttributeSet::OnRep_MovementSpeed(const FGameplayAttributeData& OldMovementSpeed)
// {
// 	GAMEPLAYATTRIBUTE_REPNOTIFY(UStrafeAttributeSet, MovementSpeed, OldMovementSpeed);
// }