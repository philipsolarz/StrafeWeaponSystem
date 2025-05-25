// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "Sound/SoundBase.h"
#include "Animation/AnimMontage.h"
#include "GameplayEffect.h"      // Required for TSubclassOf<UGameplayEffect>
#include "Abilities/GameplayAbility.h"     // Required for TSubclassOf<UGameplayAbility>
#include "GameplayTagContainer.h" // Required for FGameplayTag
#include "GameplayEffectTypes.h" // Required for FGameplayAttribute


#include "WeaponDataAsset.generated.h"

UENUM(BlueprintType)
enum class EAmmoType : uint8 // This enum might become redundant if ammo is purely attribute-based
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

    // EAmmoType AmmoType = EAmmoType::None; // Potentially redundant

    // UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) // MaxAmmo now in AttributeSet
    // int32 MaxAmmo = 50;

    // UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) // AmmoPerPickup handled by pickup's GE
    // int32 AmmoPerPickup = 10;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) // FireRate now handled by Ability's Cooldown GE
        float PrimaryFireRate = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) // FireRate now handled by Ability's Cooldown GE
        float SecondaryFireRate = 0.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    float WeaponSwitchTime = 0.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    bool bHasSecondaryFire = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TSubclassOf<class AProjectileBase> PrimaryProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TSubclassOf<class AProjectileBase> SecondaryProjectileClass; // If secondary fire also spawns a projectile

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 MaxActiveProjectiles = 0; // 0 = unlimited. This might be checked by the ability or a separate system.

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Sticky")
    bool bCanStickToCharacters = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Sticky")
    float StickyAttachmentOffset = 2.0f;
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
    USoundBase* EmptySound; // Played by UI or a specific "OutOfAmmo" feedback ability/cue

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
    UAnimationAsset* FireAnimation; // Could be used by GameplayCue for 3P anims

    // UI Properties
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    UTexture2D* CrosshairTexture;

    // Sound Properties
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound")
    USoundBase* EquipSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound")
    USoundBase* PickupSound;

    // Effect Socket Properties
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
    FName MuzzleFlashSocketName = "Muzzle";

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
    FName FireSoundSocketName = "Muzzle";

    // Weapon Attachment Properties
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Attachment")
    FName LeftHandIKSocketName;

    // Animation Properties
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
    UAnimMontage* FireMontage_1P;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
    UAnimMontage* FireMontage_3P;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
    UAnimMontage* EquipAnimation_1P;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
    UAnimMontage* EquipAnimation_3P;

    // --- GAS RELATED PROPERTIES ---

    // Abilities
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Abilities")
    TSubclassOf<UGameplayAbility> PrimaryFireAbility;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Abilities")
    TSubclassOf<UGameplayAbility> SecondaryFireAbility;

    // This tag should be unique for this specific weapon's primary fire cooldown
    // The GameplayEffect for the cooldown will also use this tag.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Cooldowns", meta = (DisplayName = "Primary Cooldown Tag"))
    FGameplayTag CooldownGameplayTag_Primary;

    // This tag should be unique for this specific weapon's secondary fire cooldown
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Cooldowns", meta = (DisplayName = "Secondary Cooldown Tag"))
    FGameplayTag CooldownGameplayTag_Secondary;

    // Ammo
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Attributes", meta = (DisplayName = "Ammo Attribute"))
    FGameplayAttribute AmmoAttribute; // e.g., UStrafeAttributeSet::GetRocketAmmoAttribute()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Attributes", meta = (DisplayName = "Max Ammo Attribute"))
    FGameplayAttribute MaxAmmoAttribute; // e.g., UStrafeAttributeSet::GetMaxRocketAmmoAttribute()

    // This GE is applied by the fire ability to consume ammo.
    // It should reduce the 'AmmoAttribute' defined above.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Costs", meta = (DisplayName = "Primary Ammo Cost Effect"))
    TSubclassOf<UGameplayEffect> AmmoCostEffect_Primary;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Costs", meta = (DisplayName = "Secondary Ammo Cost Effect"))
    TSubclassOf<UGameplayEffect> AmmoCostEffect_Secondary;

    // Initial ammo to grant when this weapon is first picked up or attributes are initialized.
    // This can be used by a GameplayEffect applied on pickup.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Attributes", meta = (DisplayName = "Initial Ammo Count"))
    float InitialAmmoCount = 10.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Attributes", meta = (DisplayName = "Default Max Ammo Value"))
    float DefaultMaxAmmo = 20.f;

    // Gameplay Cue tags
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Effects")
    FGameplayTag MuzzleFlashCueTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Effects")
    FGameplayTag ExplosionEffectCueTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Effects")
    FGameplayTag ImpactEffectCueTag; // For future hitscan weapons
};