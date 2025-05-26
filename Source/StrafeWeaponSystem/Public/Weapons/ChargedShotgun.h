// New file: Source/StrafeWeaponSystem/Public/Weapons/ChargedShotgun.h
#pragma once

#include "CoreMinimal.h"
#include "BaseWeapon.h"
#include "ChargedShotgun.generated.h"

/**
 * A hitscan weapon that can be charged for primary fire,
 * and over-charged for a powerful secondary shot.
 */
UCLASS(Blueprintable)
class STRAFEWEAPONSYSTEM_API AChargedShotgun : public ABaseWeapon
{
    GENERATED_BODY()

public:
    AChargedShotgun();

    // Add any shotgun-specific C++ functions here if needed,
    // though most logic will be in GameplayAbilities.

protected:
    virtual void BeginPlay() override;

    // Example: If you wanted to do something specific when the weapon is equipped beyond base class
    // virtual void Equip(ACharacter* NewOwner) override;
    // virtual void Unequip() override;

public:
    /**
     * Performs the hitscan logic for the shotgun blast.
     * This function is typically called by the firing Gameplay Ability.
     * @param StartLocation The starting point of the trace.
     * @param AimDirection The normalized direction of the shot.
     * @param PelletCount Number of pellets to fire.
     * @param SpreadAngle Max angle (in degrees) from the center for pellet spread.
     * @param HitscanRange Max range of the hitscan.
     * @param DamageToApply Base damage per pellet/hit.
     * @param DamageTypeClass The class of damage to apply.
     * @param InstigatorPawn The pawn that instigated this shot.
     * @param InstigatorController The controller of the instigator.
     * @param OptionalImpactCueTag A gameplay cue to play at hit locations.
     */
    UFUNCTION(BlueprintCallable, Category = "Weapon|ChargedShotgun")
    void PerformHitscanShot(
        const FVector& StartLocation,
        const FVector& AimDirection,
        int32 PelletCount,
        float SpreadAngle,
        float HitscanRange,
        float DamageToApply,
        TSubclassOf<class UDamageType> DamageTypeClass,
        APawn* InstigatorPawn,
        AController* InstigatorController,
        FGameplayTag OptionalImpactCueTag
    );
};