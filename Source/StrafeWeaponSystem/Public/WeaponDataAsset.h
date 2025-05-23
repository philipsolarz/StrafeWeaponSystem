// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "WeaponDataAsset.generated.h"

UENUM(BlueprintType)
enum class EAmmoType : uint8
{
    None,
    Rockets,
    StickyGrenades,
    // Add more as needed
};

USTRUCT(BlueprintType)
struct FWeaponStats
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FName WeaponName = "Default Weapon";

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    EAmmoType AmmoType = EAmmoType::None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 MaxAmmo = 50;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 AmmoPerPickup = 10;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    float PrimaryFireRate = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    float SecondaryFireRate = 0.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    float WeaponSwitchTime = 0.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    bool bHasSecondaryFire = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TSubclassOf<class AProjectileBase> PrimaryProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 MaxActiveProjectiles = 0; // 0 = unlimited
};

UCLASS()
class STRAFEWEAPONSYSTEM_API UWeaponDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    FWeaponStats WeaponStats;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    USkeletalMesh* WeaponMesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
    class UNiagaraSystem* MuzzleFlashEffect;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
    USoundBase* FireSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
    USoundBase* EmptySound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
    UAnimationAsset* FireAnimation;
};