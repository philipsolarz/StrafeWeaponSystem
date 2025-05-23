// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "StrafeCharacter.generated.h"

class UWeaponInventoryComponent;
class ABaseWeapon; // Forward declaration

UCLASS()
class STRAFEWEAPONSYSTEM_API AStrafeCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AStrafeCharacter();

protected:
    virtual void BeginPlay() override;

    // Weapon Inventory Component
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UWeaponInventoryComponent* WeaponInventoryComponent;

    // Example: Starting weapon class (can be set in Blueprint)
    UPROPERTY(EditDefaultsOnly, Category = "Weapons")
    TSubclassOf<ABaseWeapon> StartingWeaponClass;

public:
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    // Function to be called by input actions
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

    // Helper to get the currently equipped weapon
    ABaseWeapon* GetCurrentWeapon() const;
};