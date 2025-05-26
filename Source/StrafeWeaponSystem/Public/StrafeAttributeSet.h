#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h" 
#include "Net/UnrealNetwork.h" 
#include "StrafeAttributeSet.generated.h"

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

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- Ammo Attributes ---
	UPROPERTY(BlueprintReadOnly, Category = "Ammo", ReplicatedUsing = OnRep_RocketAmmo)
	FGameplayAttributeData RocketAmmo;
	ATTRIBUTE_ACCESSORS(UStrafeAttributeSet, RocketAmmo);

	UPROPERTY(BlueprintReadOnly, Category = "Ammo", ReplicatedUsing = OnRep_MaxRocketAmmo)
	FGameplayAttributeData MaxRocketAmmo;
	ATTRIBUTE_ACCESSORS(UStrafeAttributeSet, MaxRocketAmmo);

	UPROPERTY(BlueprintReadOnly, Category = "Ammo", ReplicatedUsing = OnRep_StickyGrenadeAmmo)
	FGameplayAttributeData StickyGrenadeAmmo;
	ATTRIBUTE_ACCESSORS(UStrafeAttributeSet, StickyGrenadeAmmo);

	UPROPERTY(BlueprintReadOnly, Category = "Ammo", ReplicatedUsing = OnRep_MaxStickyGrenadeAmmo)
	FGameplayAttributeData MaxStickyGrenadeAmmo;
	ATTRIBUTE_ACCESSORS(UStrafeAttributeSet, MaxStickyGrenadeAmmo);

	UPROPERTY(BlueprintReadOnly, Category = "Ammo", ReplicatedUsing = OnRep_ShotgunAmmo)
	FGameplayAttributeData ShotgunAmmo;
	ATTRIBUTE_ACCESSORS(UStrafeAttributeSet, ShotgunAmmo);

	UPROPERTY(BlueprintReadOnly, Category = "Ammo", ReplicatedUsing = OnRep_MaxShotgunAmmo)
	FGameplayAttributeData MaxShotgunAmmo;
	ATTRIBUTE_ACCESSORS(UStrafeAttributeSet, MaxShotgunAmmo);

	// --- Weapon State Attributes (Optional - could also be tags) ---
	// Example: Could be used if a GE applies a lockout by reducing this to 0
	// UPROPERTY(BlueprintReadOnly, Category = "WeaponState", ReplicatedUsing = OnRep_WeaponLockout)
	// FGameplayAttributeData WeaponLockout; 
	// ATTRIBUTE_ACCESSORS(UStrafeAttributeSet, WeaponLockout);


	// --- Standard Character Attributes (Examples) ---
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
	UFUNCTION()
	virtual void OnRep_RocketAmmo(const FGameplayAttributeData& OldRocketAmmo);
	UFUNCTION()
	virtual void OnRep_MaxRocketAmmo(const FGameplayAttributeData& OldMaxRocketAmmo);
	UFUNCTION()
	virtual void OnRep_StickyGrenadeAmmo(const FGameplayAttributeData& OldStickyGrenadeAmmo);
	UFUNCTION()
	virtual void OnRep_MaxStickyGrenadeAmmo(const FGameplayAttributeData& OldMaxStickyGrenadeAmmo);
	UFUNCTION()
	virtual void OnRep_ShotgunAmmo(const FGameplayAttributeData& OldShotgunAmmo);
	UFUNCTION()
	virtual void OnRep_MaxShotgunAmmo(const FGameplayAttributeData& OldMaxShotgunAmmo);

	// UFUNCTION()
	// virtual void OnRep_WeaponLockout(const FGameplayAttributeData& OldWeaponLockout);
	// UFUNCTION()
	// virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);
	// UFUNCTION()
	// virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);
	// UFUNCTION()
	// virtual void OnRep_MovementSpeed(const FGameplayAttributeData& OldMovementSpeed);
};