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
#include "GameplayEffect.h" // Required for FGameplayEffectQuery


UGA_ChargedShotgun_PrimaryFire::UGA_ChargedShotgun_PrimaryFire()
{
    AbilityInputID = 100;
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    bRetriggerInstancedAbility = true;

    bIsCharging = false;
    bChargeComplete = false;
    bInputReleasedEarly = false;

    ChargeInProgressTag = FGameplayTag::RequestGameplayTag(FName("State.Weapon.ChargedShotgun.Charging.PrimaryFire"));

    // Add tags directly to the AbilityTags member in the constructor
    AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Weapon.PrimaryFire")));
    AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Weapon.ChargedShotgun.PrimaryFire")));


    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Cooldown.Weapon.ChargedShotgun.PrimaryFire")));
}

bool UGA_ChargedShotgun_PrimaryFire::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
    UE_LOG(LogTemp, VeryVerbose, TEXT("GA_ChargedShotgun_PrimaryFire::CanActivateAbility called"));

    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_ChargedShotgun_PrimaryFire::CanActivateAbility - Super returned false"));
        return false;
    }

    AStrafeCharacter* Character = Cast<AStrafeCharacter>(ActorInfo->AvatarActor.Get());
    if (!Character || !Character->GetCurrentWeapon())
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_PrimaryFire::CanActivateAbility: No Character or Equipped Weapon."));
        return false;
    }

    AChargedShotgun* TempEquippedWeapon = Cast<AChargedShotgun>(Character->GetCurrentWeapon());
    if (!TempEquippedWeapon)
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_PrimaryFire::CanActivateAbility: Equipped weapon is not AChargedShotgun."));
        return false;
    }

    const UWeaponDataAsset* TempWeaponData = TempEquippedWeapon->GetWeaponData();
    if (!TempWeaponData)
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_PrimaryFire::CanActivateAbility: No WeaponDataAsset found on weapon."));
        return false;
    }

    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    if (ASC && TempWeaponData->AmmoAttribute.IsValid())
    {
        if (ASC->GetNumericAttribute(TempWeaponData->AmmoAttribute) <= 0)
        {
            if (OptionalRelevantTags)
            {
                OptionalRelevantTags->AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Feedback.OutOfAmmo")));
            }
            if (TempWeaponData->EmptySound)
            {
                UGameplayStatics::PlaySoundAtLocation(GetWorld(), TempWeaponData->EmptySound, Character->GetActorLocation());
            }
            return false;
        }
    }
    else if (!ASC || (TempWeaponData && !TempWeaponData->AmmoAttribute.IsValid()))
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_PrimaryFire::CanActivateAbility: ASC or AmmoAttribute is invalid. Ammo check skipped."));
    }

    FGameplayTag WeaponLockoutTag = FGameplayTag::RequestGameplayTag(FName("State.Weapon.ChargedShotgun.Lockout"));
    if (ASC && ASC->HasMatchingGameplayTag(WeaponLockoutTag))
    {
        if (OptionalRelevantTags) OptionalRelevantTags->AddTag(WeaponLockoutTag);
        return false;
    }

    return true;
}

void UGA_ChargedShotgun_PrimaryFire::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    UE_LOG(LogTemp, Warning, TEXT("GA_ChargedShotgun_PrimaryFire::ActivateAbility called"));
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    bInputReleasedEarly = false;
    bIsCharging = false;
    bChargeComplete = false;


    AStrafeCharacter* Character = GetStrafeCharacterFromActorInfo();
    if (!Character)
    {
        UE_LOG(LogTemp, Error, TEXT("GA_ChargedShotgun_PrimaryFire::ActivateAbility - No character found"));
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    EquippedWeapon = Cast<AChargedShotgun>(GetEquippedWeaponFromActorInfo());
    if (!EquippedWeapon)
    {
        UE_LOG(LogTemp, Error, TEXT("GA_ChargedShotgun_PrimaryFire::ActivateAbility - Failed to get AChargedShotgun from character"));
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

    PrimaryChargeGEClass = WeaponData->PrimaryFireChargeGE;
    EarlyReleaseCooldownGEClass = WeaponData->PrimaryFireEarlyReleaseCooldownGE;
    AmmoCostGEClass = WeaponData->AmmoCostEffect_Primary;
    CooldownTagPrimaryFire = WeaponData->CooldownGameplayTag_Primary;


    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

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

    StartCharge();
}

void UGA_ChargedShotgun_PrimaryFire::StartCharge()
{
    const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
    if (bIsCharging || !WeaponData || !ActorInfo || !ActorInfo->AbilitySystemComponent.Get())
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_PrimaryFire::StartCharge - Already charging or missing data. bIsCharging: %s"), bIsCharging ? TEXT("true") : TEXT("false"));
        return;
    }

    bIsCharging = true;
    bChargeComplete = false;

    if (PrimaryChargeGEClass)
    {
        ApplyGameplayEffectToOwner(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), PrimaryChargeGEClass.GetDefaultObject(), 1.0f);
    }

    if (WeaponData->PrimaryChargeSound)
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), WeaponData->PrimaryChargeSound, GetAvatarActorFromActorInfo()->GetActorLocation());
    }

    if (WeaponData->ChargePrimaryCueTag.IsValid() && ActorInfo->AbilitySystemComponent.Get())
    {
        FGameplayCueParameters CueParams;
        CueParams.Instigator = GetAvatarActorFromActorInfo();
        CueParams.EffectContext = ActorInfo->AbilitySystemComponent->MakeEffectContext();
        if (CueParams.EffectContext.IsValid() && EquippedWeapon) CueParams.EffectContext.AddSourceObject(EquippedWeapon);
        ActorInfo->AbilitySystemComponent->ExecuteGameplayCue(WeaponData->ChargePrimaryCueTag, CueParams);
    }

    ActorInfo->AbilitySystemComponent->AddLooseGameplayTag(ChargeInProgressTag);

    GetWorld()->GetTimerManager().SetTimer(ChargeTimerHandle, this, &UGA_ChargedShotgun_PrimaryFire::HandleFullCharge, WeaponData->WeaponStats.PrimaryChargeTime, false);

    UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_PrimaryFire: Started charging. Duration: %f"), WeaponData->WeaponStats.PrimaryChargeTime);
}

void UGA_ChargedShotgun_PrimaryFire::InputReleased(float TimeHeld)
{
    UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_PrimaryFire: Input Released after %f seconds. bIsCharging: %s, bChargeComplete: %s"),
        TimeHeld,
        bIsCharging ? TEXT("true") : TEXT("false"),
        bChargeComplete ? TEXT("true") : TEXT("false"));

    if (IsActive())
    {
        bInputReleasedEarly = true;

        if (bIsCharging && !bChargeComplete)
        {
            UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_PrimaryFire: Input released early during charge. Cancelling charge."));

            if (GetWorld() && ChargeTimerHandle.IsValid())
            {
                GetWorld()->GetTimerManager().ClearTimer(ChargeTimerHandle);
            }
            ApplyEarlyReleaseCooldown();
            ResetChargeState();
            EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
        }
    }
}

void UGA_ChargedShotgun_PrimaryFire::HandleFullCharge()
{
    UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_PrimaryFire: Charge Complete. bInputReleasedEarly state just before check: %s"), bInputReleasedEarly ? TEXT("true") : TEXT("false"));

    if (bInputReleasedEarly)
    {
        UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_PrimaryFire: Full charge timer fired, but input was already released. Aborting shot."));
        if (IsActive() && !bChargeComplete)
        {
            ResetChargeState();
            EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
        }
        return;
    }

    bChargeComplete = true;
    bIsCharging = false;

    PerformShot();
    EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UGA_ChargedShotgun_PrimaryFire::PerformShot()
{
    const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
    if (!EquippedWeapon || !WeaponData || !GetAvatarActorFromActorInfo() || !ActorInfo || !ActorInfo->AbilitySystemComponent.Get())
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_PrimaryFire::PerformShot: Missing weapon, data, avatar or ASC."));
        return;
    }

    AStrafeCharacter* Character = Cast<AStrafeCharacter>(GetAvatarActorFromActorInfo());
    if (!Character)
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_PrimaryFire::PerformShot: Missing Character."));
        return;
    }

    AController* Controller = Character->GetController();
    if (!Controller)
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_PrimaryFire::PerformShot: Missing Controller on Character."));
        return;
    }

    APlayerController* PC = Cast<APlayerController>(Controller);

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

    ApplyPrimaryFireCooldown();

    FVector MuzzleLocationForEffects;
    FRotator AimRotation;

    AimRotation = Controller->GetControlRotation();
    FVector AimDirection = AimRotation.Vector();

    if (EquippedWeapon->GetWeaponMeshComponent() && WeaponData->MuzzleFlashSocketName != NAME_None)
    {
        MuzzleLocationForEffects = EquippedWeapon->GetWeaponMeshComponent()->GetSocketLocation(WeaponData->MuzzleFlashSocketName);
    }
    else
    {
        FVector CamLoc;
        Character->GetActorEyesViewPoint(CamLoc, AimRotation);
        MuzzleLocationForEffects = CamLoc + AimDirection * 100.0f;
    }


    UAnimMontage* FireMontage1P = WeaponData->FireMontage_1P;
    UAnimMontage* FireMontage3P = WeaponData->FireMontage_3P;
    UAnimMontage* MontageToPlay = Character->IsLocallyControlled() && FireMontage1P ? FireMontage1P : FireMontage3P;
    if (MontageToPlay)
    {
        FireMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, MontageToPlay);
        if (FireMontageTask)
        {
            FireMontageTask->OnCompleted.AddDynamic(this, &UGA_ChargedShotgun_PrimaryFire::OnMontageCompleted);
            FireMontageTask->OnBlendOut.AddDynamic(this, &UGA_ChargedShotgun_PrimaryFire::OnMontageBlendOut);
            FireMontageTask->OnInterrupted.AddDynamic(this, &UGA_ChargedShotgun_PrimaryFire::OnMontageInterrupted);
            FireMontageTask->OnCancelled.AddDynamic(this, &UGA_ChargedShotgun_PrimaryFire::OnMontageCancelled);
            FireMontageTask->ReadyForActivation();
        }
    }

    float DamagePerPellet = 10.0f;
    TSubclassOf<UDamageType> DamageType = UDamageType::StaticClass();

    FVector TraceStartLocation;
    FRotator TempRot;
    if (PC) PC->GetPlayerViewPoint(TraceStartLocation, TempRot);
    else Character->GetActorEyesViewPoint(TraceStartLocation, TempRot);


    EquippedWeapon->PerformHitscanShot(
        TraceStartLocation,
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

    if (WeaponData->MuzzleFlashCueTag.IsValid() && ActorInfo->AbilitySystemComponent.Get())
    {
        FGameplayCueParameters CueParams;
        CueParams.Location = MuzzleLocationForEffects;
        CueParams.Normal = AimDirection;
        CueParams.Instigator = Character;
        CueParams.EffectContext = ActorInfo->AbilitySystemComponent->MakeEffectContext();
        if (CueParams.EffectContext.IsValid() && EquippedWeapon) CueParams.EffectContext.AddSourceObject(EquippedWeapon);
        ActorInfo->AbilitySystemComponent->ExecuteGameplayCue(WeaponData->MuzzleFlashCueTag, CueParams);
    }

    if (WeaponData->ShotgunBlastSound)
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), WeaponData->ShotgunBlastSound, MuzzleLocationForEffects);
    }
    else if (WeaponData->FireSound)
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), WeaponData->FireSound, MuzzleLocationForEffects);
    }

    UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_PrimaryFire: Shot Performed. Pellets: %d, Spread: %f"), WeaponData->WeaponStats.PrimaryPelletCount, WeaponData->WeaponStats.PrimarySpreadAngle);
}

void UGA_ChargedShotgun_PrimaryFire::ResetChargeState()
{
    const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
    if (!ActorInfo || !ActorInfo->AbilitySystemComponent.Get()) return;

    if (GetWorld() && ChargeTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(ChargeTimerHandle);
    }
    bIsCharging = false;

    if (PrimaryChargeGEClass)
    {
        FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(ChargeInProgressTag));
        ActorInfo->AbilitySystemComponent->RemoveActiveEffects(Query);
    }

    if (WeaponData && WeaponData->ChargePrimaryCueTag.IsValid())
    {
        ActorInfo->AbilitySystemComponent->RemoveGameplayCue(WeaponData->ChargePrimaryCueTag);
    }

    ActorInfo->AbilitySystemComponent->RemoveLooseGameplayTag(ChargeInProgressTag);
    UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_PrimaryFire: Charge State Reset."));
}

void UGA_ChargedShotgun_PrimaryFire::ApplyEarlyReleaseCooldown()
{
    const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
    if (EarlyReleaseCooldownGEClass && ActorInfo && ActorInfo->AbilitySystemComponent.Get())
    {
        UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_PrimaryFire: Applying early release cooldown. Duration: %f"), WeaponData ? WeaponData->WeaponStats.PrimaryEarlyReleaseCooldown : -1.0f);
        FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(EarlyReleaseCooldownGEClass);
        ApplyGameplayEffectSpecToOwner(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), SpecHandle);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_PrimaryFire: EarlyReleaseCooldownGEClass is not set in WeaponData or ASC is missing!"));
    }
}

void UGA_ChargedShotgun_PrimaryFire::ApplyPrimaryFireCooldown()
{
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (!ASC || !WeaponData || !PrimaryFireCooldownGEClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_PrimaryFire: Cannot apply primary fire cooldown. ASC: %s, WeaponData: %s, PrimaryFireCooldownGEClass: %s"),
            ASC ? TEXT("Valid") : TEXT("NULL"),
            WeaponData ? TEXT("Valid") : TEXT("NULL"),
            PrimaryFireCooldownGEClass ? TEXT("Valid") : TEXT("NULL")
        );
        return;
    }

    if (!CooldownTagPrimaryFire.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("GA_Shotgun_PrimaryFire: CooldownGameplayTag_Primary is not valid in WeaponData. Cooldown might not block correctly."));
    }

    ApplyGameplayEffectToOwner(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), PrimaryFireCooldownGEClass.GetDefaultObject(), GetAbilityLevel());
    UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_PrimaryFire: Applied primary fire cooldown GE: %s. Cooldown Tag: %s"), *PrimaryFireCooldownGEClass->GetName(), *CooldownTagPrimaryFire.ToString());
}

void UGA_ChargedShotgun_PrimaryFire::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
    UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_PrimaryFire: CancelAbility called."));

    if (GetWorld() && ChargeTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(ChargeTimerHandle);
    }

    ResetChargeState();
    if (WaitInputReleaseTask && WaitInputReleaseTask->IsActive())
    {
        WaitInputReleaseTask->EndTask();
    }
    if (FireMontageTask && FireMontageTask->IsActive())
    {
        FireMontageTask->EndTask();
    }
    Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}

void UGA_ChargedShotgun_PrimaryFire::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    UE_LOG(LogTemp, Log, TEXT("GA_Shotgun_PrimaryFire: EndAbility called. WasCancelled: %s. bIsCharging: %s, bChargeComplete: %s, bInputReleasedEarly: %s"),
        bWasCancelled ? TEXT("true") : TEXT("false"),
        bIsCharging ? TEXT("true") : TEXT("false"),
        bChargeComplete ? TEXT("true") : TEXT("false"),
        bInputReleasedEarly ? TEXT("true") : TEXT("false")
    );

    if (bIsCharging || (bWasCancelled && !bInputReleasedEarly))
    {
        ResetChargeState();
    }

    if (GetWorld() && ChargeTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(ChargeTimerHandle);
    }
    if (WaitInputReleaseTask && WaitInputReleaseTask->IsActive())
    {
        WaitInputReleaseTask->EndTask();
    }
    if (FireMontageTask && FireMontageTask->IsActive())
    {
        FireMontageTask->EndTask();
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_ChargedShotgun_PrimaryFire::OnMontageCompleted()
{
}

void UGA_ChargedShotgun_PrimaryFire::OnMontageBlendOut()
{
}

void UGA_ChargedShotgun_PrimaryFire::OnMontageInterrupted()
{
    if (IsActive())
    {
        CancelAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true);
    }
}

void UGA_ChargedShotgun_PrimaryFire::OnMontageCancelled()
{
    if (IsActive())
    {
        CancelAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true);
    }
}