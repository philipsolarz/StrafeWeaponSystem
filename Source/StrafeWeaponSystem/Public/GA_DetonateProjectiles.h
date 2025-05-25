#pragma once

#include "CoreMinimal.h"
#include "GA_WeaponActivate.h"
#include "GA_DetonateProjectiles.generated.h"

UCLASS(Abstract)
class STRAFEWEAPONSYSTEM_API UGA_DetonateProjectiles : public UGA_WeaponActivate
{
    GENERATED_BODY()

public:
    UGA_DetonateProjectiles();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
    // How many projectiles to detonate per activation (-1 = all)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Detonation")
    int32 ProjectilesToDetonate = 1;

    // Whether to detonate oldest or newest projectiles first
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Detonation")
    bool bDetonateOldestFirst = true;

    // Override in derived classes to specify which projectile type to detonate
    virtual TSubclassOf<class AProjectileBase> GetProjectileClassToDetonate() const PURE_VIRTUAL(UGA_DetonateProjectiles::GetProjectileClassToDetonate, return nullptr;);

    UFUNCTION(BlueprintImplementableEvent, Category = "Ability|Detonation")
    void K2_OnProjectilesDetonated(int32 NumDetonated);
};