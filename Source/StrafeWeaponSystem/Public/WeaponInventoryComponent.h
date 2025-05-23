// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "Components/ActorComponent.h"
#include "WeaponDataAsset.h"
#include "WeaponInventoryComponent.generated.h"

class ABaseWeapon;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponEquipped, ABaseWeapon*, NewWeapon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponAdded, TSubclassOf<ABaseWeapon>, WeaponClass);

USTRUCT(BlueprintType)
struct FAmmoReserve
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    EAmmoType AmmoType = EAmmoType::None;

    UPROPERTY(BlueprintReadOnly)
    int32 Count = 0;

    FAmmoReserve() {}
    FAmmoReserve(EAmmoType InType, int32 InCount) : AmmoType(InType), Count(InCount) {}
};

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

    UPROPERTY(ReplicatedUsing = OnRep_AmmoReserves)
    TArray<FAmmoReserve> AmmoReserves;

    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    TArray<TSubclassOf<ABaseWeapon>> StartingWeapons;

    FTimerHandle WeaponSwitchTimer;
    ABaseWeapon* PendingWeapon;

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

    // Ammo Management
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void AddAmmo(EAmmoType AmmoType, int32 Amount);

    UFUNCTION(BlueprintPure, Category = "Weapon")
    int32 GetAmmoCount(EAmmoType AmmoType) const;

    // Query Functions
    UFUNCTION(BlueprintPure, Category = "Weapon")
    ABaseWeapon* GetCurrentWeapon() const { return CurrentWeapon; }

    UFUNCTION(BlueprintPure, Category = "Weapon")
    bool HasWeapon(TSubclassOf<ABaseWeapon> WeaponClass) const;

    UFUNCTION(BlueprintPure, Category = "Weapon")
    const TArray<ABaseWeapon*>& GetWeaponInventory() const { return WeaponInventory; }

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

    UFUNCTION()
    void OnRep_AmmoReserves();

    void FinishWeaponSwitch();

    UFUNCTION(NetMulticast, Reliable)
    void MulticastEquipWeapon(ABaseWeapon* NewWeapon);

    int32 FindAmmoReserveIndex(EAmmoType AmmoType) const;
};