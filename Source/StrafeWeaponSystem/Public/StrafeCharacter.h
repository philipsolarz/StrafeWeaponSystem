// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h" // Required for FInputActionValue
#include "StrafeCharacter.generated.h"

class UWeaponInventoryComponent;
class ABaseWeapon;
class UInputAction;
class UInputMappingContext;
class USpringArmComponent; // For camera boom
class UCameraComponent;    // For camera

UCLASS()
class STRAFEWEAPONSYSTEM_API AStrafeCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AStrafeCharacter();

protected:
    virtual void BeginPlay() override;

    // COMPONENTS
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UWeaponInventoryComponent* WeaponInventoryComponent;

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


public:
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    // INPUT HANDLER FUNCTIONS
protected:
    // Movement
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    // Jump is handled by ACharacter's Jump() and StopJumping() which can be directly bound or overridden if needed.

    // Weapon
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void StartPrimaryFire();

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void StopPrimaryFire();

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void StartSecondaryFire();

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void StopSecondaryFire();

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void NextWeapon();

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void PreviousWeapon();

public:
    // Helper to get the currently equipped weapon
    UFUNCTION(BlueprintPure, Category = "Weapon")
    ABaseWeapon* GetCurrentWeapon() const;

    // Getter for WeaponInventoryComponent
    UFUNCTION(BlueprintPure, Category = "Weapon")
    UWeaponInventoryComponent* GetWeaponInventoryComponent() const { return WeaponInventoryComponent; }

};