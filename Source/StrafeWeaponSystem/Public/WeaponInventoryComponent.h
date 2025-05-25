// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
// #include "Engine/NetSerialization.h" // FAmmoReserve was removed
#include "Components/ActorComponent.h"
// #include "WeaponDataAsset.h" // EAmmoType was here, now potentially obsolete
#include "WeaponInventoryComponent.generated.h"

class ABaseWeapon;
class UGameplayAbility; // For TSubclassOf<UGameplayAbility>
class UWeaponDataAsset; // Forward declare

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponEquipped, ABaseWeapon*, NewWeapon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponAdded, TSubclassOf<ABaseWeapon>, WeaponClass); // Or ABaseWeapon* if you pass the instance

// USTRUCT(BlueprintType) // FAmmoReserve is removed
// struct FAmmoReserve
// {
//     GENERATED_BODY()
//
//     UPROPERTY(BlueprintReadOnly)
//     EAmmoType AmmoType = EAmmoType::None;
//
//     UPROPERTY(BlueprintReadOnly)
//     int32 Count = 0;
//
//     FAmmoReserve() {}
//     FAmmoReserve(EAmmoType InType, int32 InCount) : AmmoType(InType), Count(InCount) {}
// };

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class STRAFEWEAPONSYSTEM_API UWeaponInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWeaponInventoryComponent();

protected:
    UPROPERTY(ReplicatedUsing = OnRep_WeaponInventory)
    TArray<ABaseWeapon*> WeaponInventory;

    UPROPERTY(ReplicatedUsing = OnRep_CurrentWeapon)
    ABaseWeapon* CurrentWeapon;

    // UPROPERTY(ReplicatedUsing = OnRep_AmmoReserves) // Removed
    // TArray<FAmmoReserve> AmmoReserves;

    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    TArray<TSubclassOf<ABaseWeapon>> StartingWeapons; // Still relevant for initial spawn

    FTimerHandle WeaponSwitchTimer; // Still relevant for weapon switch delay
    ABaseWeapon* PendingWeapon;     // Still relevant

public:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Weapon Management
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    bool AddWeapon(TSubclassOf<ABaseWeapon> WeaponClass);

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void EquipWeapon(TSubclassOf<ABaseWeapon> WeaponClass);

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void EquipWeaponByIndex(int32 Index);

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void NextWeapon();

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void PreviousWeapon();

    UFUNCTION(Server, Reliable)
    void ServerAddWeapon(TSubclassOf<ABaseWeapon> WeaponClass);

    UFUNCTION(Server, Reliable)
    void ServerEquipWeapon(TSubclassOf<ABaseWeapon> WeaponClass);

    // Ammo Management - These are removed as ammo is handled by Character's AttributeSet
    // UFUNCTION(BlueprintCallable, Category = "Weapon")
    // void AddAmmo(EAmmoType AmmoType, int32 Amount);
    //
    // UFUNCTION(BlueprintPure, Category = "Weapon")
    // int32 GetAmmoCount(EAmmoType AmmoType) const;

    // Query Functions
    UFUNCTION(BlueprintPure, Category = "Weapon")
    ABaseWeapon* GetCurrentWeapon() const { return CurrentWeapon; }

    UFUNCTION(BlueprintPure, Category = "Weapon")
    bool HasWeapon(TSubclassOf<ABaseWeapon> WeaponClass) const;

    UFUNCTION(BlueprintPure, Category = "Weapon")
    const TArray<ABaseWeapon*>& GetWeaponInventoryList() const { return WeaponInventory; } // Renamed for clarity

    // Events
    UPROPERTY(BlueprintAssignable, Category = "Weapon")
    FOnWeaponEquipped OnWeaponEquipped;

    UPROPERTY(BlueprintAssignable, Category = "Weapon")
    FOnWeaponAdded OnWeaponAdded;

protected:
    UFUNCTION()
    void OnRep_CurrentWeapon();

    UFUNCTION()
    void OnRep_WeaponInventory();

    // UFUNCTION() // Removed
    // void OnRep_AmmoReserves();

    void FinishWeaponSwitch(); // Still relevant

    UFUNCTION(NetMulticast, Reliable) // Still relevant for notifying clients of equip
        void MulticastEquipWeaponVisuals(ABaseWeapon* NewWeapon); // Renamed for clarity, actual ability granting is server-side on Character

    // int32 FindAmmoReserveIndex(EAmmoType AmmoType) const; // Removed
};