#include "GA_WeaponFire.h"
#include "StrafeCharacter.h"
#include "BaseWeapon.h"
#include "WeaponDataAsset.h"
#include "ProjectileBase.h"
#include "StrafeAttributeSet.h" // For checking ammo
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h" // For GetAbilitySystemComponent
#include "Kismet/GameplayStatics.h" // For sound and effect spawning (can be moved to GameplayCues)
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "DrawDebugHelpers.h"


UGA_WeaponFire::UGA_WeaponFire()
{
	// This tag will be used to identify this ability (e.g., for input binding)
	// It's often good practice to assign this in the Blueprint derivative though.
	// AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Weapon.PrimaryFire")));

	// ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dead")));
	// ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Stunned")));
}

bool UGA_WeaponFire::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AStrafeCharacter* Character = Cast<AStrafeCharacter>(ActorInfo->AvatarActor.Get());
	if (!Character || !ActorInfo->AbilitySystemComponent.Get()) // Also check ASC validity
	{
		return false;
	}

	const ABaseWeapon* Weapon = Character->GetCurrentWeapon();
	if (!Weapon || !Weapon->GetWeaponData())
	{
		return false;
	}

	const UWeaponDataAsset* WeaponData = Weapon->GetWeaponData();
	// Accessing PrimaryProjectileClass via WeaponStats
	if (!WeaponData->WeaponStats.PrimaryProjectileClass) // <<<<<<< CORRECTED ACCESS
	{
		UE_LOG(LogTemp, Warning, TEXT("UGA_WeaponFire::CanActivateAbility: No PrimaryProjectileClass set in WeaponData for %s"), *Weapon->GetName());
		return false;
	}

	// Check Ammo Attribute
	// const UStrafeAttributeSet* Attributes = Character->GetAttributeSet(); // Use getter if AttributeSet is private/protected
	const UStrafeAttributeSet* Attributes = Character->AttributeSet.Get(); // Direct access if public

	if (Attributes && WeaponData->AmmoAttribute.IsValid())
	{
		// Use ASC to get attribute value
		float CurrentAmmo = ActorInfo->AbilitySystemComponent->GetNumericAttribute(WeaponData->AmmoAttribute); // <<<<<<< CORRECTED
		if (CurrentAmmo <= 0)
		{
			return false;
		}
	}
	else if (WeaponData->AmmoAttribute.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UGA_WeaponFire::CanActivateAbility: AttributeSet is null on character %s but weapon %s expects ammo attribute %s."), *Character->GetName(), *Weapon->GetName(), *WeaponData->AmmoAttribute.GetName());
		return false;
	}
	return true;
}

void UGA_WeaponFire::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    AStrafeCharacter* Character = GetStrafeCharacterFromActorInfo();
    ABaseWeapon* Weapon = GetEquippedWeaponFromActorInfo();

    if (!Character || !Weapon || !Weapon->GetWeaponData() || !Weapon->GetWeaponData()->WeaponStats.PrimaryProjectileClass)
    {
        UE_LOG(LogTemp, Error, TEXT("UGA_WeaponFire::ActivateAbility: Invalid Character, Weapon, WeaponData, or PrimaryProjectileClass."));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    const UWeaponDataAsset* LocalWeaponData = Weapon->GetWeaponData();

    // Get Muzzle Location & Rotation
    FVector MuzzleLocation = Weapon->GetActorLocation() + Weapon->GetActorForwardVector() * 100.0f;
    FRotator MuzzleRotation = Weapon->GetActorRotation();
    USkeletalMeshComponent* WeaponMesh = Weapon->GetWeaponMeshComponent();

    if (WeaponMesh && LocalWeaponData && WeaponMesh->DoesSocketExist(LocalWeaponData->MuzzleFlashSocketName))
    {
        MuzzleLocation = WeaponMesh->GetSocketLocation(LocalWeaponData->MuzzleFlashSocketName);
        MuzzleRotation = WeaponMesh->GetSocketRotation(LocalWeaponData->MuzzleFlashSocketName);
    }

    // Use character's aim direction
    if (AController* Controller = Character->GetController())
    {
        MuzzleRotation = Controller->GetControlRotation();

        FVector CameraLocation;
        FRotator CameraRotation;
        Controller->GetPlayerViewPoint(CameraLocation, CameraRotation);

        FVector AimDirection = MuzzleRotation.Vector();
        FVector TraceStart = CameraLocation;
        FVector TraceEnd = TraceStart + (AimDirection * 10000.0f);

        FHitResult HitResult;
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(Weapon);
        QueryParams.AddIgnoredActor(Character);

        bool bHit = GetWorld()->LineTraceSingleByChannel(
            HitResult,
            TraceStart,
            TraceEnd,
            ECC_Visibility,
            QueryParams
        );

        MuzzleLocation = TraceStart + AimDirection * 150.0f;

        if (bHit && HitResult.GetActor() != Character && HitResult.GetActor() != Weapon)
        {
            MuzzleRotation = (HitResult.ImpactPoint - MuzzleLocation).Rotation();
        }

#if WITH_EDITOR
        if (GetWorld()->GetNetMode() != NM_DedicatedServer)
        {
            DrawDebugLine(GetWorld(), MuzzleLocation, MuzzleLocation + MuzzleRotation.Vector() * 1000.0f, FColor::Green, false, 1.0f, 0, 1.0f);
            if (bHit) DrawDebugSphere(GetWorld(), HitResult.ImpactPoint, 15.f, 12, FColor::Red, false, 1.f, 0, 1.f);
        }
#endif
    }

    // Spawn Projectile
    SpawnProjectile(Weapon, MuzzleLocation, MuzzleRotation);

    // Execute Muzzle Flash Gameplay Cue
    if (LocalWeaponData->MuzzleFlashCueTag.IsValid() && ActorInfo->AbilitySystemComponent.IsValid())
    {
        FGameplayCueParameters CueParams;
        CueParams.Location = MuzzleLocation;
        CueParams.Normal = MuzzleRotation.Vector();
        CueParams.SourceObject = Weapon;

        // Store the attachment info in the effect context
        FGameplayEffectContextHandle ContextHandle = ActorInfo->AbilitySystemComponent->MakeEffectContext();
        ContextHandle.AddSourceObject(Weapon);
        CueParams.EffectContext = ContextHandle;

        ActorInfo->AbilitySystemComponent->ExecuteGameplayCue(LocalWeaponData->MuzzleFlashCueTag, CueParams);
    }

    // Trigger Blueprint event for additional effects
    K2_OnWeaponFired();

    EndAbility(GetCurrentAbilitySpecHandle(), CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_WeaponFire::CommitExecute(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::CommitExecute(Handle, ActorInfo, ActivationInfo);
	// This is where you'd apply costs and cooldowns if not using CommitAbility in ActivateAbility.
	// Since CommitAbility handles this, this function might be empty or used for post-commit logic.
	// For example, if you want to ensure cost is paid BEFORE firing:
	// ApplyCost(Handle, ActorInfo, ActivationInfo);
	// ApplyCooldown(Handle, ActorInfo, ActivationInfo);
}


const UWeaponDataAsset* UGA_WeaponFire::GetWeaponData() const
{
	ABaseWeapon* Weapon = GetEquippedWeaponFromActorInfo();
	return Weapon ? Weapon->GetWeaponData() : nullptr;
}

void UGA_WeaponFire::SpawnProjectile_Implementation(ABaseWeapon* Weapon, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	if (!Weapon || !Weapon->GetWeaponData() || !Weapon->GetWeaponData()->WeaponStats.PrimaryProjectileClass) return;

	const UWeaponDataAsset* WeaponData = Weapon->GetWeaponData();
	UWorld* World = GetWorld();
	if (!World) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Weapon->GetOwner();
	SpawnParams.Instigator = Cast<APawn>(Weapon->GetOwner());
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AProjectileBase* Projectile = World->SpawnActor<AProjectileBase>(
		WeaponData->WeaponStats.PrimaryProjectileClass,
		SpawnLocation,
		SpawnRotation,
		SpawnParams
	);

	if (Projectile)
	{
		// Pass WeaponData to projectile
		Projectile->InitializeProjectile(Cast<AController>(Weapon->GetOwner()->GetInstigatorController()), Weapon, WeaponData);
		UE_LOG(LogTemp, Log, TEXT("UGA_WeaponFire: Projectile %s spawned by %s"), *Projectile->GetName(), *GetNameSafe(Weapon->GetOwner()));
	}
}