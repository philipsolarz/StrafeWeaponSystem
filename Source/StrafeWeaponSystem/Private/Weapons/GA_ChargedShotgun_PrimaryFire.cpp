// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapons/GA_ChargedShotgun_PrimaryFire.h"
#include "Weapons/ChargedShotgun.h" // Specific weapon
#include "WeaponDataAsset.h" // Access to weapon stats and GEs
#include "StrafeCharacter.h" // Or your base character class
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayTagsManager.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h" // For playing sounds directly if not handled by cues
#include "GameFramework/PlayerController.h"


UGA_ChargedShotgun_PrimaryFire::UGA_ChargedShotgun_PrimaryFire()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated; // Primary fire logic is server-authoritative
    bRetriggerInstancedAbility = true; // Allows re-triggering if input is held for continuous charge

    bIsCharging = false;
    bChargeComplete = false;
    bInputReleasedEarly = false;

    // Define default tags - these should be replaced by tags from WeaponDataAsset if possible
    // Or defined in DefaultGameplayTags.ini and loaded
    ChargeInProgressTag = FGameplayTag::RequestGameplayTag(FName("State.Weapon.Charging.Primary"));
    // CooldownTagPrimaryFire will be set from WeaponDataAsset

    // It's good practice to add relevant tags to the ability itself

    AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Weapon.PrimaryFire")));
    AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Weapon.Shotgun.Primary")));
}

bool UGA_ChargedShotgun_PrimaryFire::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        return false;
    }

    AStrafeCharacter* Character = Cast<AStrafeCharacter>(ActorInfo->AvatarActor.Get());
    if (!Character || !Character->GetCurrentWeapon())
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_PrimaryFire::CanActivateAbility: No Character or Equipped Weapon."));
        return false;
    }

    EquippedWeapon = Cast<AChargedShotgun>(Character->GetCurrentWeapon());
    if (!EquippedWeapon)
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_PrimaryFire::CanActivateAbility: Equipped weapon is not AChargedShotgun."));
        return false;
    }

    WeaponData = EquippedWeapon->GetWeaponData();
    if (!WeaponData)
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_PrimaryFire::CanActivateAbility: No WeaponDataAsset found on weapon."));
        return false;
    }

    // Check for ammo using the attribute from WeaponDataAsset
    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    if (ASC && WeaponData->AmmoAttribute.IsValid())
    {
        if (ASC->GetNumericAttribute(WeaponData->AmmoAttribute) <= 0)
        {
            // Optional: Add a tag like "Ability.Feedback.OutOfAmmo" to OptionalRelevantTags
            // This could trigger a UI response or sound.
            if (OptionalRelevantTags)
            {
                OptionalRelevantTags->AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Feedback.OutOfAmmo")));
            }
            // Play empty sound from WeaponData if available
            if (WeaponData->EmptySound)
            {
                UGameplayStatics::PlaySoundAtLocation(GetWorld(), WeaponData->EmptySound, Character->GetActorLocation());
            }
            return false;
        }
    }
    else if (!ASC || !WeaponData->AmmoAttribute.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_PrimaryFire::CanActivateAbility: ASC or AmmoAttribute is invalid. Ammo check skipped."));
    }


    // Check if the weapon is locked out by another ability (e.g. secondary fire cooldown)
    // This requires a "Weapon.Lockout" tag to be defined and checked.
    FGameplayTag WeaponLockoutTag = FGameplayTag::RequestGameplayTag(FName("State.Weapon.Lockout"));
    if (ASC && ASC->HasMatchingGameplayTag(WeaponLockoutTag))
    {
        if (OptionalRelevantTags) OptionalRelevantTags->AddTag(WeaponLockoutTag);
        return false;
    }


    return true;
}

void UGA_ChargedShotgun_PrimaryFire::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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
        UE_LOG(LogTemp, Error, TEXT("GA_Shotgun_PrimaryFire: Failed to cast to AChargedShotgun."));
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    WeaponData = EquippedWeapon->GetWeaponData();
    if (!WeaponData)
    {
        UE_LOG(LogTemp, Error, TEXT("GA_Shotgun_PrimaryFire: WeaponData is null."));
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    // Cache GEs and Tags from WeaponData
    PrimaryChargeGEClass = WeaponData->PrimaryFireChargeGE;
    EarlyReleaseCooldownGEClass = WeaponData->PrimaryFireEarlyReleaseCooldownGE;
    AmmoCostGEClass = WeaponData->AmmoCostEffect_Primary;
    CooldownTagPrimaryFire = WeaponData->CooldownGameplayTag_Primary; // Used to find the Cooldown GE

    // Find the Cooldown GE for primary fire. This assumes the Cooldown GE has the CooldownTagPrimaryFire tag.
    // A more robust way might be to store TSubclassOf<UGameplayEffect> CooldownPrimaryFireGE directly in WeaponDataAsset.
    // For now, we'll create one if not found, or expect it to be set up in BP.
    // For simplicity, let's assume a direct TSubclassOf property will be added or this logic is handled in BP version of the ability.

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    // Listen for input release
    WaitInputReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this);
    if (WaitInputReleaseTask)
    {
        WaitInputReleaseTask->OnRelease.AddDynamic(this, &UGA_ChargedShotgun_PrimaryFire::InputReleased);
        WaitInputReleaseTask->ReadyForActivation();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("GA_Shotgun_PrimaryFire: WaitInputReleaseTask failed to create."));
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    bInputReleasedEarly = false; // Reset flag at the start of activation
    StartCharge();
}

void UGA_ChargedShotgun_PrimaryFire::StartCharge()
{
    if (bIsCharging || !WeaponData || !GetActorInfo()->AbilitySystemComponent.Get())
    {
        return;
    }

    bIsCharging = true;
    bChargeComplete = false;

    // Apply charging GameplayEffect
    if (PrimaryChargeGEClass)
    {
        ApplyGameplayEffectToOwner(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), PrimaryChargeGEClass.GetDefaultObject(), 1.0f);
    }

    // Play charging sound from WeaponDataAsset
    if (WeaponData->PrimaryChargeSound)
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), WeaponData->PrimaryChargeSound, GetAvatarActorFromActorInfo()->GetActorLocation());
    }

    // Activate charge GameplayCue
    if (WeaponData->ChargePrimaryCueTag.IsValid() && GetActorInfo()->AbilitySystemComponent.Get())
    {
        FGameplayCueParameters CueParams;
        CueParams.Instigator = GetAvatarActorFromActorInfo();
        CueParams.EffectContext = GetActorInfo()->AbilitySystemComponent->MakeEffectContext();
        CueParams.EffectContext.AddSourceObject(EquippedWeapon);
        GetActorInfo()->AbilitySystemComponent->ExecuteGameplayCue(WeaponData->ChargePrimaryCueTag, CueParams);
    }

    GetActorInfo()->AbilitySystemComponent->AddLooseGameplayTag(ChargeInProgressTag);

    // Start timer for full charge
    GetWorld()->GetTimerManager().SetTimer(ChargeTimerHandle, this, &UGA_ChargedShotgun_PrimaryFire::HandleFullCharge, WeaponData->WeaponStats.PrimaryChargeTime, false);

    UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_PrimaryFire: Started charging. Duration: %f"), WeaponData->WeaponStats.PrimaryChargeTime);
}

void UGA_ChargedShotgun_PrimaryFire::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
    UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_PrimaryFire: Input Released."));
    bInputReleasedEarly = true; // Mark that input was released

    if (bIsCharging && !bChargeComplete)
    {
        UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_PrimaryFire: Input released early during charge. Cancelling charge."));
        ApplyEarlyReleaseCooldown();
        ResetChargeState(); // This will clear timer, remove GE, stop cue
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false); // Not a cancel, but a normal end due to early release
        return;
    }

    // If charge is complete AND input is released, the shot has already been fired by HandleFullCharge
    // or is about to be fired if HandleFullCharge didn't auto-fire.
    // The main purpose of InputReleased after full charge is to stop continuous charging.
    // If HandleFullCharge already fired and called EndAbility, this path might not even be hit for that instance.
    // If this ability is set to bRetriggerInstancedAbility, a new instance might be started if input is held.
    // This logic might need refinement based on exact continuous fire feel.

    // Clear continuous charge timer if it was set
    GetWorld()->GetTimerManager().ClearTimer(ContinuousChargeDelayTimerHandle);
}


void UGA_ChargedShotgun_PrimaryFire::HandleFullCharge()
{
    UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_PrimaryFire: Charge Complete."));
    bChargeComplete = true;

    if (bInputReleasedEarly)
    {
        // Input was released just before this timer fired.
        // The InputReleased function should have handled cooldown and ending.
        UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_PrimaryFire: Full charge reached, but input was released just prior. Aborting shot."));
        // ApplyEarlyReleaseCooldown(); // This might be redundant if InputReleased already did it.
        // ResetChargeState();
        // EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
        return; // InputReleased should handle the termination.
    }

    // If we reach here, input is still held (or was released *exactly* when this fired).
    PerformShot();
    ResetChargeState(); // Stop current charge cues/GEs

    // Check if input is still held to attempt continuous charge
    AController* Controller = GetActorInfo()->PlayerController.Get();
    if (Controller && Controller->IsInputKeyDown(CurrentActivationInfo.ActivationKey)) // Check if the specific activation key is still down
    {
        UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_PrimaryFire: Input still held after full charge shot. Attempting continuous charge."));
        // Instead of directly restarting, end this instance and let bRetriggerInstancedAbility handle it,
        // or use a short delay task to re-evaluate.
        // For "full automatic" feel, we might want to immediately try to re-activate or re-charge.
        // For simplicity here, we'll end and rely on bRetriggerInstancedAbility.
        // If we want a slight delay or more control:
        // GetWorld()->GetTimerManager().SetTimer(ContinuousChargeDelayTimerHandle, this, &UGA_ChargedShotgun_PrimaryFire::AttemptContinuousCharge, 0.01f, false);

        // End current ability instance. If input is still held, a new instance should be triggered if bRetriggerInstancedAbility = true
        EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_PrimaryFire: Input released after full charge shot or not held. Ending ability."));
        EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
    }
}

void UGA_ChargedShotgun_PrimaryFire::AttemptContinuousCharge()
{
    // This function would be called by a timer if we wanted a more controlled re-charge loop.
    // For now, relying on bRetriggerInstancedAbility and ending the current ability.
    // If called, it would check CanActivate and then call ActivateAbility or StartCharge directly.
    // This logic might be better handled by the ability system's retriggering.
}


void UGA_ChargedShotgun_PrimaryFire::PerformShot()
{
    if (!EquippedWeapon || !WeaponData || !GetAvatarActorFromActorInfo() || !GetActorInfo()->AbilitySystemComponent.Get())
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_PrimaryFire::PerformShot: Missing weapon, data, or avatar."));
        return;
    }

    AStrafeCharacter* Character = Cast<AStrafeCharacter>(GetAvatarActorFromActorInfo());
    AController* Controller = Character ? Character->GetController() : nullptr;

    if (!Character || !Controller)
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_PrimaryFire::PerformShot: Missing Character or Controller."));
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
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_PrimaryFire: AmmoCostGEClass is not set in WeaponData!"));
    }

    // Apply Primary Fire Cooldown (this is the weapon's intrinsic fire rate, not the early release penalty)
    ApplyPrimaryFireCooldown();


    // Get muzzle location and aim direction
    FVector MuzzleLocation;
    FRotator AimRotation;
    Character->GetActorEyesViewPoint(MuzzleLocation, AimRotation); // Default to eye height for aim

    // Adjust MuzzleLocation to actual weapon muzzle if a socket is defined
    if (EquippedWeapon->GetWeaponMeshComponent() && WeaponData->MuzzleFlashSocketName != NAME_None)
    {
        MuzzleLocation = EquippedWeapon->GetWeaponMeshComponent()->GetSocketLocation(WeaponData->MuzzleFlashSocketName);
    }
    FVector AimDirection = AimRotation.Vector();


    // Play Fire Montage
    UAnimMontage* FireMontage1P = WeaponData->FireMontage_1P;
    UAnimMontage* FireMontage3P = WeaponData->FireMontage_3P; // TODO: Actually use 3P montage on non-owned characters.

    UAnimMontage* MontageToPlay = Character->IsLocallyControlled() && FireMontage1P ? FireMontage1P : FireMontage3P;
    if (MontageToPlay)
    {
        FireMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, MontageToPlay);
        FireMontageTask->OnCompleted.AddDynamic(this, &UGA_ChargedShotgun_PrimaryFire::OnMontageCompleted);
        FireMontageTask->OnBlendOut.AddDynamic(this, &UGA_ChargedShotgun_PrimaryFire::OnMontageBlendOut);
        FireMontageTask->OnInterrupted.AddDynamic(this, &UGA_ChargedShotgun_PrimaryFire::OnMontageInterrupted);
        FireMontageTask->OnCancelled.AddDynamic(this, &UGA_ChargedShotgun_PrimaryFire::OnMontageCancelled);
        FireMontageTask->ReadyForActivation();
    }


    // Execute shot
    // For simplicity, damage is hardcoded here. Ideally, this comes from WeaponData or a GE.
    // A common pattern is to have a base damage on WeaponData and then apply a GE that reads that.
    // For now, let's assume a placeholder damage value.
    float DamagePerPellet = 10.0f; // Placeholder - should be data-driven
    TSubclassOf<UDamageType> DamageType = UDamageType::StaticClass(); // Placeholder

    EquippedWeapon->PerformHitscanShot(
        MuzzleLocation,
        AimDirection,
        WeaponData->WeaponStats.PrimaryPelletCount,
        WeaponData->WeaponStats.PrimarySpreadAngle,
        WeaponData->WeaponStats.PrimaryHitscanRange,
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
        CueParams.Normal = AimDirection; // For orientation if needed
        CueParams.Instigator = Character;
        CueParams.EffectContext = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
        CueParams.EffectContext.AddSourceObject(EquippedWeapon);
        GetAbilitySystemComponentFromActorInfo()->ExecuteGameplayCue(WeaponData->MuzzleFlashCueTag, CueParams, GetAbilitySystemComponentFromActorInfo()->ScopedPredictionKey);
    }

    // Play fire sound (already part of MuzzleFlashCueTag handling ideally, or can be played directly)
    if (WeaponData->ShotgunBlastSound) // Use specific shotgun blast sound
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), WeaponData->ShotgunBlastSound, MuzzleLocation);
    }
    else if (WeaponData->FireSound) // Fallback to generic fire sound
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), WeaponData->FireSound, MuzzleLocation);
    }


    UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_PrimaryFire: Shot Performed. Pellets: %d, Spread: %f"), WeaponData->WeaponStats.PrimaryPelletCount, WeaponData->WeaponStats.PrimarySpreadAngle);
}

void UGA_ChargedShotgun_PrimaryFire::ResetChargeState()
{
    if (!GetActorInfo()->AbilitySystemComponent.Get()) return;

    GetWorld()->GetTimerManager().ClearTimer(ChargeTimerHandle);
    bIsCharging = false;
    bChargeComplete = false; // Reset for next potential charge

    // Remove charging GameplayEffect
    if (PrimaryChargeGEClass)
    {
        FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(PrimaryChargeGEClass.GetDefaultObject()->GetGrantedTags());
        GetActorInfo()->AbilitySystemComponent->RemoveActiveEffects(Query);
    }

    // Stop charge GameplayCue (usually by removing the tag or sending a "stop" event)
    // For tag-based cues, removing the tag is enough.
    // For event-based, you might need to send CueTag.Remove or similar.
    if (WeaponData && WeaponData->ChargePrimaryCueTag.IsValid())
    {
        GetActorInfo()->AbilitySystemComponent->RemoveGameplayCue(WeaponData->ChargePrimaryCueTag);
        // Or if it's a stateful cue, ensure it's stopped:
        // FGameplayCueParameters CueParams; // Fill if needed
        // GetActorInfo()->AbilitySystemComponent->ExecuteGameplayCue(UGameplayCueManager::RemoveGameplayCueEffectTag, CueParams); // This is not quite right.
        // GameplayCues are often fire-and-forget or managed by their own Blueprint logic based on tags.
        // Removing the ChargeInProgressTag should be the primary way to stop visual cues.
    }

    GetActorInfo()->AbilitySystemComponent->RemoveLooseGameplayTag(ChargeInProgressTag);
    UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_PrimaryFire: Charge State Reset."));
}

void UGA_ChargedShotgun_PrimaryFire::ApplyEarlyReleaseCooldown()
{
    if (EarlyReleaseCooldownGEClass && GetActorInfo()->AbilitySystemComponent.Get())
    {
        UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_PrimaryFire: Applying early release cooldown. Duration: %f"), WeaponData ? WeaponData->WeaponStats.PrimaryEarlyReleaseCooldown : -1.0f);
        FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(EarlyReleaseCooldownGEClass);
        // The GE itself should define the duration based on WeaponData->WeaponStats.PrimaryEarlyReleaseCooldown
        // If the GE is scalable with SetByCaller, you can set it here:
        // SpecHandle.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(FName("Data.Cooldown.Duration")), WeaponData->WeaponStats.PrimaryEarlyReleaseCooldown);
        ApplyGameplayEffectSpecToOwner(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), SpecHandle);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_PrimaryFire: EarlyReleaseCooldownGEClass is not set in WeaponData!"));
    }
}

void UGA_ChargedShotgun_PrimaryFire::ApplyPrimaryFireCooldown()
{
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (!ASC || !WeaponData || !WeaponData->CooldownGameplayTag_Primary.IsValid()) return;

    // A common way is to have a generic "ApplyCooldown" GE, and then set its duration and tags by caller.
    // Or, each weapon defines its specific Cooldown GE in its WeaponDataAsset.
    // For simplicity, let's assume there's a UGameplayEffect TSubclass defined in Blueprint for this
    // that already contains the CooldownGameplayTag_Primary.
    // The WeaponDataAsset does not store the GE for primary fire cooldown, only the tag.
    // This ability would need a UPROPERTY(EditDefaultsOnly, Category="Cooldown") TSubclassOf<UGameplayEffect> PrimaryFireCooldownGE
    // that the designer sets in the Blueprint version of this ability.
    // This GE would then apply the CooldownGameplayTag_Primary for its duration.

    // For now, we'll log if we can't find a GE to apply.
    // This is a point that needs proper setup in BP or a more direct GE reference.
    if (PrimaryFireCooldownGEClass) // Assuming we add this property and set it.
    {
        ApplyGameplayEffectToOwner(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), PrimaryFireCooldownGEClass.GetDefaultObject(), GetAbilityLevel());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_PrimaryFire: PrimaryFireCooldownGEClass is not set for this ability. Cooldown tag: %s"), *WeaponData->CooldownGameplayTag_Primary.ToString());
        // As a fallback, we can manually apply the cooldown tag via a generic GE if one exists.
        // Or the designer *must* create a GE that applies this tag and assign it to the ability.
    }
}

void UGA_ChargedShotgun_PrimaryFire::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
    UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_PrimaryFire: CancelAbility called."));
    ResetChargeState();
    if (WaitInputReleaseTask)
    {
        WaitInputReleaseTask->EndTask();
    }
    if (FireMontageTask)
    {
        FireMontageTask->EndTask();
    }
    GetWorld()->GetTimerManager().ClearTimer(ContinuousChargeDelayTimerHandle);
    Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}

void UGA_ChargedShotgun_PrimaryFire::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_PrimaryFire: EndAbility called. WasCancelled: %s"), bWasCancelled ? TEXT("true") : TEXT("false"));
    if (bWasCancelled) // Ensure state is clean if cancelled mid-charge
    {
        ResetChargeState();
    }

    if (WaitInputReleaseTask)
    {
        WaitInputReleaseTask->EndTask();
    }
    if (FireMontageTask)
    {
        FireMontageTask->EndTask();
    }
    GetWorld()->GetTimerManager().ClearTimer(ContinuousChargeDelayTimerHandle);
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}


// --- Montage Callbacks ---
void UGA_ChargedShotgun_PrimaryFire::OnMontageCompleted()
{
    // If continuous fire isn't handled by bRetriggerInstancedAbility, you might re-evaluate here or end.
    // For now, PerformShot's logic handles the end or re-trigger.
    // EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UGA_ChargedShotgun_PrimaryFire::OnMontageBlendOut()
{
    // Similar to OnMontageCompleted
    // EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UGA_ChargedShotgun_PrimaryFire::OnMontageInterrupted()
{
    // End the ability if the montage is interrupted.
    CancelAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true);
}

void UGA_ChargedShotgun_PrimaryFire::OnMontageCancelled()
{
    // End the ability if the montage is cancelled.
    CancelAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true);
}