#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h" // Required for UAbilitySystemComponent
#include "Net/UnrealNetwork.h" // Required for DOREPLIFETIME
#include "StrafeAttributeSet.generated.h"

// Uses macros from AttributeSet.h
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class STRAFEWEAPONSYSTEM_API UStrafeAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UStrafeAttributeSet();

	// AttributeSet Overrides
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Ammo attributes
	// Example: Rocket Launcher
	UPROPERTY(BlueprintReadOnly, Category = "Ammo", ReplicatedUsing = OnRep_RocketAmmo)
	FGameplayAttributeData RocketAmmo;
	ATTRIBUTE_ACCESSORS(UStrafeAttributeSet, RocketAmmo);

	UPROPERTY(BlueprintReadOnly, Category = "Ammo", ReplicatedUsing = OnRep_MaxRocketAmmo)
	FGameplayAttributeData MaxRocketAmmo;
	ATTRIBUTE_ACCESSORS(UStrafeAttributeSet, MaxRocketAmmo);

	// Example: Sticky Grenade Launcher
	UPROPERTY(BlueprintReadOnly, Category = "Ammo", ReplicatedUsing = OnRep_StickyGrenadeAmmo)
	FGameplayAttributeData StickyGrenadeAmmo;
	ATTRIBUTE_ACCESSORS(UStrafeAttributeSet, StickyGrenadeAmmo);

	UPROPERTY(BlueprintReadOnly, Category = "Ammo", ReplicatedUsing = OnRep_MaxStickyGrenadeAmmo)
	FGameplayAttributeData MaxStickyGrenadeAmmo;
	ATTRIBUTE_ACCESSORS(UStrafeAttributeSet, MaxStickyGrenadeAmmo);

	// Placeholder for other attributes you might add
	// UPROPERTY(BlueprintReadOnly, Category = "Health", ReplicatedUsing = OnRep_Health)
	// FGameplayAttributeData Health;
	// ATTRIBUTE_ACCESSORS(UStrafeAttributeSet, Health);

	// UPROPERTY(BlueprintReadOnly, Category = "Health", ReplicatedUsing = OnRep_MaxHealth)
	// FGameplayAttributeData MaxHealth;
	// ATTRIBUTE_ACCESSORS(UStrafeAttributeSet, MaxHealth);

	// UPROPERTY(BlueprintReadOnly, Category = "Movement", ReplicatedUsing = OnRep_MovementSpeed)
	// FGameplayAttributeData MovementSpeed;
	// ATTRIBUTE_ACCESSORS(UStrafeAttributeSet, MovementSpeed);


protected:
	// Replication functions
	UFUNCTION()
	virtual void OnRep_RocketAmmo(const FGameplayAttributeData& OldRocketAmmo);

	UFUNCTION()
	virtual void OnRep_MaxRocketAmmo(const FGameplayAttributeData& OldMaxRocketAmmo);

	UFUNCTION()
	virtual void OnRep_StickyGrenadeAmmo(const FGameplayAttributeData& OldStickyGrenadeAmmo);

	UFUNCTION()
	virtual void OnRep_MaxStickyGrenadeAmmo(const FGameplayAttributeData& OldMaxStickyGrenadeAmmo);

	// UFUNCTION()
	// virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);
	//
	// UFUNCTION()
	// virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);
	//
	// UFUNCTION()
	// virtual void OnRep_MovementSpeed(const FGameplayAttributeData& OldMovementSpeed);
};