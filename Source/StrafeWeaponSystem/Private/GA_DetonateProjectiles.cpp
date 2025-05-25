#include "GA_DetonateProjectiles.h"
#include "StrafeCharacter.h"
#include "BaseWeapon.h"
#include "ProjectileBase.h"
#include "WeaponInventoryComponent.h"

UGA_DetonateProjectiles::UGA_DetonateProjectiles()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

bool UGA_DetonateProjectiles::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        return false;
    }

    ABaseWeapon* Weapon = GetEquippedWeaponFromActorInfo();
    if (!Weapon || Weapon->GetActiveProjectiles().Num() == 0)
    {
        return false;
    }

    // Check if we have any projectiles of the correct type
    TSubclassOf<AProjectileBase> ProjectileClass = GetProjectileClassToDetonate();
    if (!ProjectileClass)
    {
        return false;
    }

    for (AProjectileBase* Projectile : Weapon->GetActiveProjectiles())
    {
        if (Projectile && Projectile->IsA(ProjectileClass))
        {
            return true;
        }
    }

    return false;
}

void UGA_DetonateProjectiles::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ABaseWeapon* Weapon = GetEquippedWeaponFromActorInfo();
    if (!Weapon)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    TSubclassOf<AProjectileBase> ProjectileClass = GetProjectileClassToDetonate();
    if (!ProjectileClass)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Get projectiles of the correct type
    TArray<AProjectileBase*> ProjectilesToProcess;
    for (AProjectileBase* Projectile : Weapon->GetActiveProjectiles())
    {
        if (Projectile && Projectile->IsA(ProjectileClass))
        {
            ProjectilesToProcess.Add(Projectile);
        }
    }

    // Sort by creation time if needed
    if (bDetonateOldestFirst)
    {
        ProjectilesToProcess.Sort([](const AProjectileBase& A, const AProjectileBase& B)
            {
                return A.GetGameTimeSinceCreation() > B.GetGameTimeSinceCreation();
            });
    }
    else
    {
        ProjectilesToProcess.Sort([](const AProjectileBase& A, const AProjectileBase& B)
            {
                return A.GetGameTimeSinceCreation() < B.GetGameTimeSinceCreation();
            });
    }

    // Detonate projectiles
    int32 NumToDetonate = ProjectilesToDetonate < 0 ? ProjectilesToProcess.Num() : FMath::Min(ProjectilesToDetonate, ProjectilesToProcess.Num());
    int32 NumDetonated = 0;

    for (int32 i = 0; i < NumToDetonate; i++)
    {
        if (ProjectilesToProcess[i])
        {
            ProjectilesToProcess[i]->Detonate();
            NumDetonated++;
        }
    }

    K2_OnProjectilesDetonated(NumDetonated);

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}