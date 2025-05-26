// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapons/GA_ChargedShotgun_SecondaryFire.h"
#include "Weapons/ChargedShotgun.h"
#include "WeaponDataAsset.h"
#include "StrafeCharacter.h" // Replace with your actual character class
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayTagsManager.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "GameFramework/PlayerController.h"

UGA_ChargedShotgun_SecondaryFire::UGA_ChargedShotgun_SecondaryFire()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated; // Server controls charging & firing

    bIsCharging = false;
    bOverchargedShotStored = false;
    bInputWasReleasedDuringCharge = false;

    // Define default tags - these can be overridden or confirmed by tags from WeaponDataAsset
    ChargeSecondaryInProgressTag = FGameplayTag::RequestGameplayTag(FName("State.Weapon.Charging.Secondary"));
    OverchargedStateTag = FGameplayTag::RequestGameplayTag(FName("State.Weapon.Overcharged"));
    WeaponLockoutTag = FGameplayTag::RequestGameplayTag(FName("State.Weapon.Lockout"));

    // Add relevant tags to the ability itself
    AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Weapon.SecondaryFire")));
    AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Weapon.Shotgun.Secondary")));
}

bool UGA_ChargedShotgun_SecondaryFire::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        return false;
    }

    AStrafeCharacter* Character = Cast<AStrafeCharacter>(ActorInfo->AvatarActor.Get());
    if (!Character || !Character->GetCurrentWeapon())
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_SecondaryFire::CanActivateAbility: No Character or Equipped Weapon."));
        return false;
    }

    // Use a temporary variable to cast and check weapon type, avoid setting member EquippedWeapon yet
    AChargedShotgun* TempWeapon = Cast<AChargedShotgun>(Character->GetCurrentWeapon());
    if (!TempWeapon)
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_SecondaryFire::CanActivateAbility: Equipped weapon is not AChargedShotgun."));
        return false;
    }

    const UWeaponDataAsset* TempWeaponData = TempWeapon->GetWeaponData();
    if (!TempWeaponData)
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_SecondaryFire::CanActivateAbility: No WeaponDataAsset found on weapon."));
        return false;
    }

    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    if (!ASC)
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_SecondaryFire::CanActivateAbility: AbilitySystemComponent is null."));
        return false;
    }

    // Check for ammo
    if (TempWeaponData->AmmoAttribute.IsValid())
    {
        if (ASC->GetNumericAttribute(TempWeaponData->AmmoAttribute) <= 0)
        {
            if (OptionalRelevantTags) OptionalRelevantTags->AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Feedback.OutOfAmmo")));
            if (TempWeaponData->EmptySound) UGameplayStatics::PlaySoundAtLocation(GetWorld(), TempWeaponData->EmptySound, Character->GetActorLocation());
            return false;
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_SecondaryFire::CanActivateAbility: AmmoAttribute is invalid in WeaponData. Ammo check skipped."));
    }

    // Check if the weapon is locked out (e.g., by this ability's own cooldown)
    if (ASC->HasMatchingGameplayTag(WeaponLockoutTag))
    {
        if (OptionalRelevantTags) OptionalRelevantTags->AddTag(WeaponLockoutTag);
        UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_SecondaryFire::CanActivateAbility: Weapon is locked out by tag %s."), *WeaponLockoutTag.ToString());
        return false;
    }

    return true;
}

void UGA_ChargedShotgun_SecondaryFire::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    AStrafeCharacter* Character = Cast<AStrafeCharacter>(ActorInfo->AvatarActor.Get());
    if (!Character)
    {
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    EquippedWeapon = Cast<AChargedShotgun>(Character->GetCurrentWeapon());
    if (!EquippedWeapon)
    {
        UE_LOG(LogTemp, Error, TEXT("GA_Shotgun_SecondaryFire: Failed to cast to AChargedShotgun in ActivateAbility."));
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    WeaponData = EquippedWeapon->GetWeaponData();
    if (!WeaponData)
    {
        UE_LOG(LogTemp, Error, TEXT("GA_Shotgun_SecondaryFire: WeaponData is null in ActivateAbility."));
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    // Cache GEs from WeaponData
    SecondaryChargeGEClass = WeaponData->SecondaryFireChargeGE;
    WeaponLockoutGEClass = WeaponData->SecondaryFireWeaponLockoutGE; // This GE must apply the WeaponLockoutTag
    AmmoCostGEClass = WeaponData->AmmoCostEffect_Secondary;

    if (!SecondaryChargeGEClass || !WeaponLockoutGEClass || !AmmoCostGEClass)
    {
        UE_LOG(LogTemp, Error, TEXT("GA_Shotgun_SecondaryFire: One or more required GEs are not set in WeaponDataAsset!"));
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    // Listen for input release
    WaitInputReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this);
    if (WaitInputReleaseTask)
    {
        WaitInputReleaseTask->OnRelease.AddDynamic(this, &UGA_ChargedShotgun_SecondaryFire::InputReleased);
        WaitInputReleaseTask->ReadyForActivation();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("GA_Shotgun_SecondaryFire: WaitInputReleaseTask failed to create."));
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    bInputWasReleasedDuringCharge = false; // Reset flag
    StartSecondaryCharge();
}

void UGA_ChargedShotgun_SecondaryFire::StartSecondaryCharge()
{
    if (bIsCharging || !WeaponData || !GetAbilitySystemComponentFromActorInfo())
    {
        return;
    }

    bIsCharging = true;
    bOverchargedShotStored = false; // Ensure it's reset if somehow re-charging

    // Apply charging GameplayEffect
    if (SecondaryChargeGEClass)
    {
        ApplyGameplayEffectToOwner(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), SecondaryChargeGEClass.GetDefaultObject(), GetAbilityLevel());
    }

    // Play charging sound
    if (WeaponData->SecondaryChargeSound)
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), WeaponData->SecondaryChargeSound, GetAvatarActorFromActorInfo()->GetActorLocation());
    }

    // Activate charge GameplayCue
    if (WeaponData->ChargeSecondaryCueTag.IsValid())
    {
        FGameplayCueParameters CueParams;
        CueParams.Instigator = GetAvatarActorFromActorInfo();
        CueParams.EffectContext = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
        CueParams.EffectContext.AddSourceObject(EquippedWeapon);
        GetAbilitySystemComponentFromActorInfo()->ExecuteGameplayCue(WeaponData->ChargeSecondaryCueTag, CueParams);
    }

    GetAbilitySystemComponentFromActorInfo()->AddLooseGameplayTag(ChargeSecondaryInProgressTag);

    // Start timer for full charge
    GetWorld()->GetTimerManager().SetTimer(SecondaryChargeTimerHandle, this, &UGA_ChargedShotgun_SecondaryFire::HandleSecondaryFullCharge, WeaponData->WeaponStats.SecondaryChargeTime, false);
    UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_SecondaryFire: Started secondary charging. Duration: %f"), WeaponData->WeaponStats.SecondaryChargeTime);
}

void UGA_ChargedShotgun_SecondaryFire::HandleSecondaryFullCharge()
{
    UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_SecondaryFire: Secondary Charge Complete."));
    bIsCharging = false; // Charging phase is done

    if (bInputWasReleasedDuringCharge) // Check if input was released *before* this timer fired
    {
        UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_SecondaryFire: Input was released before full secondary charge. Aborting."));
        ResetAbilityState(); // Clean up charge GEs/tags/cues
        EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false); // End normally, no penalty
        return;
    }

    bOverchargedShotStored = true;

    // Remove charging GameplayEffect and tag
    if (SecondaryChargeGEClass)
    {
        FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(SecondaryChargeGEClass.GetDefaultObject()->GetGrantedTags());
        GetAbilitySystemComponentFromActorInfo()->RemoveActiveEffects(Query);
    }
    GetAbilitySystemComponentFromActorInfo()->RemoveLooseGameplayTag(ChargeSecondaryInProgressTag);
    if (WeaponData->ChargeSecondaryCueTag.IsValid()) // Stop charging cue
    {
        GetAbilitySystemComponentFromActorInfo()->RemoveGameplayCue(WeaponData->ChargeSecondaryCueTag);
    }

    // Activate "Overcharged" GameplayCue and Tag
    if (WeaponData->OverchargedCueTag.IsValid())
    {
        FGameplayCueParameters CueParams;
        CueParams.Instigator = GetAvatarActorFromActorInfo();
        CueParams.EffectContext = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
        CueParams.EffectContext.AddSourceObject(EquippedWeapon);
        GetAbilitySystemComponentFromActorInfo()->ExecuteGameplayCue(WeaponData->OverchargedCueTag, CueParams);
    }
    GetAbilitySystemComponentFromActorInfo()->AddLooseGameplayTag(OverchargedStateTag);

    // Play a sound indicating full charge if desired (could be part of OverchargedCueTag)
    // For example: if (WeaponData->SecondaryChargeCompleteSound) UGameplayStatics::PlaySound...

    UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_SecondaryFire: Overcharged shot is stored. Waiting for input release."));
    // The ability remains active, waiting for InputReleased to fire.
}

void UGA_ChargedShotgun_SecondaryFire::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
    UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_SecondaryFire: Input Released. bIsCharging: %d, bOverchargedShotStored: %d"), bIsCharging, bOverchargedShotStored);

    if (bOverchargedShotStored)
    {
        // We have a stored shot, fire it.
        AttemptFireOverchargedShot();
        // AttemptFireOverchargedShot will call EndAbility internally.
    }
    else if (bIsCharging)
    {
        // Input released mid-charge, before full charge was reached.
        UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_SecondaryFire: Input released during secondary charge. Aborting charge."));
        bInputWasReleasedDuringCharge = true; // Set flag for HandleSecondaryFullCharge if it fires very close to this
        ResetAbilityState(); // Stop charge sounds/visuals, clear timer, remove GEs/tags.
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false); // End normally, no penalty.
    }
    else
    {
        // Input released, but no charge was active and no shot was stored.
        // This might happen if the ability was cancelled or ended for other reasons before input release.
        UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_SecondaryFire: Input released, but no charge active or shot stored. Ending."));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UGA_ChargedShotgun_SecondaryFire::AttemptFireOverchargedShot()
{
    if (!EquippedWeapon || !WeaponData || !GetAvatarActorFromActorInfo() || !GetAbilitySystemComponentFromActorInfo())
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_SecondaryFire::AttemptFireOverchargedShot: Missing critical components."));
        CancelAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true);
        return;
    }

    // Final ammo check before firing
    if (WeaponData->AmmoAttribute.IsValid() && GetAbilitySystemComponentFromActorInfo()->GetNumericAttribute(WeaponData->AmmoAttribute) <= 0)
    {
        UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_SecondaryFire::AttemptFireOverchargedShot: Out of ammo just before firing."));
        if (WeaponData->EmptySound) UGameplayStatics::PlaySoundAtLocation(GetWorld(), WeaponData->EmptySound, GetAvatarActorFromActorInfo()->GetActorLocation());
        ResetAbilityState(false); // Reset visual state of charge but don't clear timers if any were about to fire (though they shouldn't be)
        CancelAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true); // Cancel as we can't fire
        return;
    }

    // Apply Ammo Cost
    if (AmmoCostGEClass)
    {
        FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(AmmoCostGEClass, GetAbilityLevel());
        if (SpecHandle.IsValid())
        {
            ApplyGameplayEffectSpecToOwner(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), SpecHandle);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_SecondaryFire: AmmoCostGEClass for secondary fire is not set in WeaponData!"));
    }

    AStrafeCharacter* Character = Cast<AStrafeCharacter>(GetAvatarActorFromActorInfo());
    AController* Controller = Character ? Character->GetController() : nullptr;

    if (!Character || !Controller)
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_SecondaryFire::AttemptFireOverchargedShot: Missing Character or Controller."));
        CancelAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true);
        return;
    }

    // Play Fire Montage
    UAnimMontage* FireMontage1P = WeaponData->FireMontage_1P;
    UAnimMontage* FireMontage3P = WeaponData->FireMontage_3P;
    UAnimMontage* MontageToPlay = Character->IsLocallyControlled() && FireMontage1P ? FireMontage1P : FireMontage3P;

    if (MontageToPlay)
    {
        FireMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, MontageToPlay);
        FireMontageTask->OnCompleted.AddDynamic(this, &UGA_ChargedShotgun_SecondaryFire::OnMontageCompleted);
        FireMontageTask->OnBlendOut.AddDynamic(this, &UGA_ChargedShotgun_SecondaryFire::OnMontageBlendOut);
        FireMontageTask->OnInterrupted.AddDynamic(this, &UGA_ChargedShotgun_SecondaryFire::OnMontageInterrupted);
        FireMontageTask->OnCancelled.AddDynamic(this, &UGA_ChargedShotgun_SecondaryFire::OnMontageCancelled);
        FireMontageTask->ReadyForActivation();
    }

    // Get muzzle location and aim direction
    FVector MuzzleLocation;
    FRotator AimRotation;
    Character->GetActorEyesViewPoint(MuzzleLocation, AimRotation);
    if (EquippedWeapon->GetWeaponMeshComponent() && WeaponData->MuzzleFlashSocketName != NAME_None)
    {
        MuzzleLocation = EquippedWeapon->GetWeaponMeshComponent()->GetSocketLocation(WeaponData->MuzzleFlashSocketName);
    }
    FVector AimDirection = AimRotation.Vector();

    // Perform the hitscan shot
    float DamagePerPellet = 15.0f; // Placeholder - should be data-driven from WeaponData for secondary
    TSubclassOf<UDamageType> DamageType = UDamageType::StaticClass(); // Placeholder

    EquippedWeapon->PerformHitscanShot(
        MuzzleLocation,
        AimDirection,
        WeaponData->WeaponStats.SecondaryPelletCount,
        WeaponData->WeaponStats.SecondarySpreadAngle,
        WeaponData->WeaponStats.SecondaryHitscanRange,
        DamagePerPellet,
        DamageType,
        Character,
        Controller,
        WeaponData->ImpactEffectCueTag
    );

    // Play Muzzle Flash GameplayCue
    if (WeaponData->MuzzleFlashCueTag.IsValid())
    {
        FGameplayCueParameters CueParams;
        CueParams.Location = MuzzleLocation;
        CueParams.Normal = AimDirection;
        CueParams.Instigator = Character;
        CueParams.EffectContext = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
        CueParams.EffectContext.AddSourceObject(EquippedWeapon);
        GetAbilitySystemComponentFromActorInfo()->ExecuteGameplayCue(WeaponData->MuzzleFlashCueTag, CueParams, GetAbilitySystemComponentFromActorInfo()->ScopedPredictionKey);
    }

    // Play fire sound
    if (WeaponData->ShotgunBlastSound) UGameplayStatics::PlaySoundAtLocation(GetWorld(), WeaponData->ShotgunBlastSound, MuzzleLocation);
    else if (WeaponData->FireSound) UGameplayStatics::PlaySoundAtLocation(GetWorld(), WeaponData->FireSound, MuzzleLocation);

    UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_SecondaryFire: Overcharged Shot Fired. Pellets: %d, Spread: %f"), WeaponData->WeaponStats.SecondaryPelletCount, WeaponData->WeaponStats.SecondarySpreadAngle);

    ApplyWeaponLockoutCooldown();

    // Clean up "Overcharged" state
    GetAbilitySystemComponentFromActorInfo()->RemoveLooseGameplayTag(OverchargedStateTag);
    if (WeaponData->OverchargedCueTag.IsValid())
    {
        GetAbilitySystemComponentFromActorInfo()->RemoveGameplayCue(WeaponData->OverchargedCueTag);
    }
    bOverchargedShotStored = false;

    // End the ability after firing. If montage is playing, it will continue.
    // If we need to wait for montage, EndAbility should be called in OnMontageCompleted/BlendOut.
    // For now, assume firing is immediate and cooldown starts now.
    EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UGA_ChargedShotgun_SecondaryFire::ApplyWeaponLockoutCooldown()
{
    if (WeaponLockoutGEClass && GetAbilitySystemComponentFromActorInfo())
    {
        UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_SecondaryFire: Applying weapon lockout cooldown. Duration from GE. Tag: %s"), *WeaponLockoutTag.ToString());
        FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(WeaponLockoutGEClass);
        // The GE itself should define the duration (WeaponData->WeaponStats.SecondaryFireCooldown)
        // and apply the WeaponLockoutTag.
        ApplyGameplayEffectSpecToOwner(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), SpecHandle);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_SecondaryFire: WeaponLockoutGEClass is not set in WeaponData or ASC is null!"));
    }
}

void UGA_ChargedShotgun_SecondaryFire::ResetAbilityState(bool bClearTimers)
{
    if (bClearTimers)
    {
        GetWorld()->GetTimerManager().ClearTimer(SecondaryChargeTimerHandle);
    }

    bIsCharging = false;
    // bOverchargedShotStored is reset specifically when fired or charge aborted.
    // bInputWasReleasedDuringCharge is reset at the start of activation.

    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (!ASC) return;

    // Remove charging GameplayEffect if active
    if (SecondaryChargeGEClass && ASC->GetAppliedGameplayEffectSpec(GetCurrentAbilitySpecHandle())) // Check if GE from this ability is active
    {
        FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(SecondaryChargeGEClass.GetDefaultObject()->GetGrantedTags());
        ASC->RemoveActiveEffects(Query);
    }

    // Remove tags and cues
    if (ASC->HasMatchingGameplayTag(ChargeSecondaryInProgressTag))
    {
        ASC->RemoveLooseGameplayTag(ChargeSecondaryInProgressTag);
        if (WeaponData && WeaponData->ChargeSecondaryCueTag.IsValid()) ASC->RemoveGameplayCue(WeaponData->ChargeSecondaryCueTag);
    }
    if (ASC->HasMatchingGameplayTag(OverchargedStateTag))
    {
        ASC->RemoveLooseGameplayTag(OverchargedStateTag);
        if (WeaponData && WeaponData->OverchargedCueTag.IsValid()) ASC->RemoveGameplayCue(WeaponData->OverchargedCueTag);
    }

    UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_SecondaryFire: Ability State Reset."));
}

void UGA_ChargedShotgun_SecondaryFire::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_SecondaryFire: EndAbility called. WasCancelled: %s"), bWasCancelled ? TEXT("true") : TEXT("false"));

    // Ensure all states are cleaned up, especially if cancelled.
    // If not cancelled, some states (like Overcharged) are cleaned up during the fire sequence.
    ResetAbilityState(); // This will clear timers and GEs/Tags

    if (WaitInputReleaseTask)
    {
        WaitInputReleaseTask->EndTask();
    }
    if (FireMontageTask)
    {
        // If ability ends before montage, montage should be stopped.
        // If montage is meant to complete, EndAbility should be called from montage callbacks.
        FireMontageTask->EndTask();
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_ChargedShotgun_SecondaryFire::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
    UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_SecondaryFire: CancelAbility called."));
    ResetAbilityState(); // Ensure everything is clean

    if (WaitInputReleaseTask)
    {
        WaitInputReleaseTask->EndTask();
    }
    if (FireMontageTask)
    {
        FireMontageTask->EndTask();
    }
    Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}

// --- Montage Callbacks ---
void UGA_ChargedShotgun_SecondaryFire::OnMontageCompleted()
{
    // If ability logic is tied to montage finishing (e.g. cooldown applied after anim), do it here.
    // For now, assuming cooldowns/etc are applied when shot is initiated.
    // If the ability hasn't ended yet, this could be a place to call EndAbility.
    // However, current logic ends ability after firing, montage might still be blending out.
}

void UGA_ChargedShotgun_SecondaryFire::OnMontageBlendOut()
{
    // Similar to OnMontageCompleted.
}

void UGA_ChargedShotgun_SecondaryFire::OnMontageInterrupted()
{
    // If montage is interrupted, it's safest to cancel the ability to ensure clean state.
    CancelAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true);
}

void UGA_ChargedShotgun_SecondaryFire::OnMontageCancelled()
{
    // Same as interrupted.
    CancelAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true);
}