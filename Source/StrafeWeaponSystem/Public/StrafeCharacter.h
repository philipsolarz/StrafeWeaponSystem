// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h" // Required for FInputActionValue
#include "AbilitySystemInterface.h" // Required for IAbilitySystemInterface
#include "GameplayTagContainer.h" // Required for FGameplayTag 
#include "GameplayAbilitySpec.h"
#include "StrafeCharacter.generated.h"

class UWeaponInventoryComponent;
class ABaseWeapon;
class UInputAction;
class UInputMappingContext;
class UAbilitySystemComponent; // Forward declaration
class UStrafeAttributeSet;   // Forward declaration
class UGameplayEffect;       // Forward declaration
class UGameplayAbility;      // Forward declaration
class UGA_WeaponActivate;    // Forward declaration for AbilityCDO

UCLASS()
class STRAFEWEAPONSYSTEM_API AStrafeCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AStrafeCharacter();

	//~ Begin IAbilitySystemInterface
	/** Returns our Ability System Component. */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~ End IAbilitySystemInterface

	UPROPERTY()
	TObjectPtr<UStrafeAttributeSet> AttributeSet;

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;


	// COMPONENTS
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UWeaponInventoryComponent* WeaponInventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Abilities, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	// WEAPON PROPERTIES
	UPROPERTY(EditDefaultsOnly, Category = "Weapons")
	TSubclassOf<ABaseWeapon> StartingWeaponClass;

	// INPUT MAPPING CONTEXTS
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputMappingContext* MovementMappingContext; // For movement and look actions

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputMappingContext* WeaponMappingContext; // For weapon-related actions

	// INPUT ACTIONS
	// Movement
	UPROPERTY(EditDefaultsOnly, Category = "Input|Movement")
	UInputAction* MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Movement")
	UInputAction* LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Movement")
	UInputAction* JumpAction;

	// Weapon
	UPROPERTY(EditDefaultsOnly, Category = "Input|Weapon")
	UInputAction* PrimaryFireAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Weapon")
	UInputAction* SecondaryFireAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Weapon")
	UInputAction* NextWeaponAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Weapon")
	UInputAction* PreviousWeaponAction;

	// Gameplay Tags for input actions (currently not used for direct activation with new method)
	// UPROPERTY(EditDefaultsOnly, Category = "Input|GameplayTags")
	// FGameplayTag PrimaryFireInputTag;

	// UPROPERTY(EditDefaultsOnly, Category = "Input|GameplayTags")
	// FGameplayTag SecondaryFireInputTag;


public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintCallable, Category = "Abilities")
	void InitializeAttributes();

	UFUNCTION(BlueprintCallable, Category = "Abilities")
	void GiveDefaultAbilities();

	// Called when a weapon is equipped by the inventory component
	UFUNCTION()
	void OnWeaponEquipped(ABaseWeapon* NewWeapon);


protected:
	// INPUT HANDLER FUNCTIONS
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	void Input_PrimaryFire_Pressed();
	void Input_PrimaryFire_Released();
	void Input_SecondaryFire_Pressed();
	void Input_SecondaryFire_Released();


	// Weapon Switching
	void NextWeapon();
	void PreviousWeapon();

public:
	// Helper to get the currently equipped weapon
	UFUNCTION(BlueprintPure, Category = "Weapon")
	ABaseWeapon* GetCurrentWeapon() const;

	// Getter for WeaponInventoryComponent
	UFUNCTION(BlueprintPure, Category = "Weapon")
	UWeaponInventoryComponent* GetWeaponInventoryComponent() const { return WeaponInventoryComponent; }

private:
	// Store handles to granted weapon abilities to be able to remove them
	TArray<FGameplayAbilitySpecHandle> CurrentWeaponAbilityHandles;

	// Default attribute values
	UPROPERTY(EditDefaultsOnly, Category = "Abilities|Defaults")
	TSubclassOf<UGameplayEffect> DefaultAttributesEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Abilities|Defaults")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	// Store current input IDs for equipped weapon abilities
	int32 CurrentPrimaryFireInputID;
	int32 CurrentSecondaryFireInputID;
};