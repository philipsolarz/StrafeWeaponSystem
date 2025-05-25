#include "WeaponInventoryComponent.h"
#include "BaseWeapon.h"
#include "WeaponDataAsset.h" // For accessing WeaponData on AddWeapon
#include "StrafeCharacter.h" // To get AbilitySystemComponent
#include "AbilitySystemComponent.h" // For applying GEs
#include "GameplayEffectTypes.h" // For FGameplayEffectContextHandle
#include "Net/UnrealNetwork.h"
#include "Engine/ActorChannel.h"
#include "Engine/Engine.h" 

UWeaponInventoryComponent::UWeaponInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UWeaponInventoryComponent::BeginPlay()
{
    Super::BeginPlay();

    // StartingWeapons are added by AStrafeCharacter now after ASC is initialized.
    // If you want inventory to manage this independently, ensure ASC is valid on owner.
}

void UWeaponInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UWeaponInventoryComponent, WeaponInventory);
    DOREPLIFETIME(UWeaponInventoryComponent, CurrentWeapon);
    // DOREPLIFETIME(UWeaponInventoryComponent, AmmoReserves); // Removed
}

bool UWeaponInventoryComponent::AddWeapon(TSubclassOf<ABaseWeapon> WeaponClass)
{
    if (!WeaponClass)
    {
        UE_LOG(LogTemp, Error, TEXT("AddWeapon called with null WeaponClass"));
        return false;
    }

    AActor* OwnerActor = GetOwner();
    if (!OwnerActor) return false;

    if (!OwnerActor->HasAuthority())
    {
        ServerAddWeapon(WeaponClass);
        // Client predicts success but actual add/ammo is server-authoritative
        // For UI, it might be okay to assume success and wait for replication,
        // or have a more complex prediction system.
        return true;
    }

    UE_LOG(LogTemp, Warning, TEXT("Server AddWeapon called for: %s"), *WeaponClass->GetName());

    // Check if we already have this weapon type
    for (ABaseWeapon* Weapon : WeaponInventory)
    {
        if (Weapon && Weapon->IsA(WeaponClass))
        {
            // Weapon already owned. Ammo pickups should be separate items that apply a GE directly.
            // If this AddWeapon call is from a "full weapon pickup" that also gives ammo:
            AStrafeCharacter* Character = Cast<AStrafeCharacter>(OwnerActor);
            UWeaponDataAsset* WeaponData = Weapon->GetWeaponData();
            if (Character && Character->GetAbilitySystemComponent() && WeaponData && WeaponData->AmmoAttribute.IsValid())
            {
                // This is a "weapon pickup that also acts as an ammo top-up".
                // You'd have a GE specifically for "adding X ammo to Y attribute".
                // For simplicity, let's assume a generic "add some ammo" effect or define one in WeaponData.
                // Example: Create a GE like "GE_AddDefaultAmmo_Rocket"
                // TSubclassOf<UGameplayEffect> AmmoTopUpEffect = WeaponData->PickupGrantAmmoEffect; // New field in UWeaponDataAsset
                // if (AmmoTopUpEffect) {
                //     FGameplayEffectContextHandle ContextHandle = Character->GetAbilitySystemComponent()->MakeEffectContext();
                //     ContextHandle.AddSourceObject(this);
                //     Character->GetAbilitySystemComponent()->ApplyGameplayEffectToSelf(AmmoTopUpEffect->GetDefaultObject(), 1.0f, ContextHandle);
                //     UE_LOG(LogTemp, Log, TEXT("Weapon %s already owned, applied ammo top-up effect."), *WeaponClass->GetName());
                // } else {
                UE_LOG(LogTemp, Log, TEXT("Weapon %s already owned. No specific ammo top-up GE defined on pickup. Ammo handled by separate ammo pickups."), *WeaponClass->GetName());
                // }
            }
            return false; // Already have the weapon type
        }
    }

    // Spawn new weapon
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = OwnerActor;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    FVector SpawnLocation = OwnerActor->GetActorLocation() + FVector(0, 0, 100); // Away from ground
    FRotator SpawnRotation = FRotator::ZeroRotator;

    ABaseWeapon* NewWeapon = GetWorld()->SpawnActor<ABaseWeapon>(WeaponClass, SpawnLocation, SpawnRotation, SpawnParams);
    if (NewWeapon)
    {
        UE_LOG(LogTemp, Warning, TEXT("Successfully spawned weapon: %s"), *NewWeapon->GetName());
        WeaponInventory.Add(NewWeapon);
        NewWeapon->SetOwner(OwnerActor);
        NewWeapon->AttachToComponent(OwnerActor->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        NewWeapon->SetActorHiddenInGame(true); // Hide until equipped

        OnWeaponAdded.Broadcast(WeaponClass); // Or NewWeapon
        OnRep_WeaponInventory(); // Force server-side update for rep notifies if any logic depends on it immediately

        // Initialize ammo for the new weapon using its WeaponDataAsset settings
        AStrafeCharacter* Character = Cast<AStrafeCharacter>(OwnerActor);
        UWeaponDataAsset* WeaponData = NewWeapon->GetWeaponData();
        UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;

        if (ASC && WeaponData && WeaponData->AmmoAttribute.IsValid() && WeaponData->MaxAmmoAttribute.IsValid())
        {
            // Create a dynamic GameplayEffect to set initial and max ammo
            UGameplayEffect* AmmoInitEffect = NewObject<UGameplayEffect>(GetTransientPackage(), FName(TEXT("AmmoInitEffect")));
            AmmoInitEffect->DurationPolicy = EGameplayEffectDurationType::Instant;

            // Modifier for Initial Ammo
            int32 InitialAmmoModIndex = AmmoInitEffect->Modifiers.Num();
            AmmoInitEffect->Modifiers.SetNum(InitialAmmoModIndex + 1);
            FGameplayModifierInfo& ModInitialAmmo = AmmoInitEffect->Modifiers[InitialAmmoModIndex];
            ModInitialAmmo.Attribute = WeaponData->AmmoAttribute;
            ModInitialAmmo.ModifierOp = EGameplayModOp::Override; // Or Add if you want to add to existing
            ModInitialAmmo.ModifierMagnitude = FScalableFloat(WeaponData->InitialAmmoCount);

            // Modifier for Max Ammo
            int32 MaxAmmoModIndex = AmmoInitEffect->Modifiers.Num();
            AmmoInitEffect->Modifiers.SetNum(MaxAmmoModIndex + 1);
            FGameplayModifierInfo& ModMaxAmmo = AmmoInitEffect->Modifiers[MaxAmmoModIndex];
            ModMaxAmmo.Attribute = WeaponData->MaxAmmoAttribute;
            ModMaxAmmo.ModifierOp = EGameplayModOp::Override;
            ModMaxAmmo.ModifierMagnitude = FScalableFloat(WeaponData->DefaultMaxAmmo);

            FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
            ContextHandle.AddSourceObject(NewWeapon); // Source is the weapon itself
            ASC->ApplyGameplayEffectToSelf(AmmoInitEffect, 1.0f, ContextHandle);

            UE_LOG(LogTemp, Log, TEXT("Applied initial ammo (%f) and max ammo (%f) for %s via dynamic GE."), WeaponData->InitialAmmoCount, WeaponData->DefaultMaxAmmo, *WeaponData->AmmoAttribute.GetName());
        }
        else
        {
            if (!ASC)
            {
                UE_LOG(LogTemp, Warning, TEXT("AddWeapon: Character or ASC is null. Cannot initialize ammo for %s."), *WeaponClass->GetName());
            }
            else if (!WeaponData) // Chained else if
            {
                UE_LOG(LogTemp, Warning, TEXT("AddWeapon: WeaponData is null for %s. Cannot initialize ammo."), *WeaponClass->GetName());
            }
            else if (!WeaponData->AmmoAttribute.IsValid() || !WeaponData->MaxAmmoAttribute.IsValid()) // Chained else if
            {
                UE_LOG(LogTemp, Log, TEXT("AddWeapon: %s does not use standard ammo attributes or they are not set in its WeaponDataAsset."), *WeaponClass->GetName());
            }
        }
        return true;
    }
    UE_LOG(LogTemp, Error, TEXT("Failed to spawn weapon actor for %s"), *WeaponClass->GetName());
    return false;
}

void UWeaponInventoryComponent::ServerAddWeapon_Implementation(TSubclassOf<ABaseWeapon> WeaponClass)
{
    AddWeapon(WeaponClass);
}

void UWeaponInventoryComponent::EquipWeapon(TSubclassOf<ABaseWeapon> WeaponClass)
{
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor || !OwnerActor->HasAuthority())
    {
        if (OwnerActor) ServerEquipWeapon(WeaponClass);
        // Client might predict the switch visually, but ability granting is server-auth
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Server EquipWeapon called for: %s"), WeaponClass ? *WeaponClass->GetName() : TEXT("nullptr"));

    if (!WeaponClass) // Unequip all
    {
        if (CurrentWeapon)
        {
            // Character's OnWeaponEquipped(nullptr) will handle clearing abilities
            CurrentWeapon->Unequip();
        }
        CurrentWeapon = nullptr;
        OnRep_CurrentWeapon(); // Notify clients
        OnWeaponEquipped.Broadcast(nullptr); // Notify local systems on server + character
        return;
    }

    ABaseWeapon* WeaponToEquip = nullptr;
    for (ABaseWeapon* Weapon : WeaponInventory)
    {
        if (Weapon && Weapon->IsA(WeaponClass))
        {
            WeaponToEquip = Weapon;
            break;
        }
    }

    if (!WeaponToEquip)
    {
        UE_LOG(LogTemp, Error, TEXT("Weapon not found in inventory: %s"), *WeaponClass->GetName());
        return;
    }
    if (WeaponToEquip == CurrentWeapon && WeaponToEquip->IsEquipped()) // Already equipped
    {
        UE_LOG(LogTemp, Log, TEXT("Weapon %s already equipped."), *WeaponClass->GetName());
        return;
    }

    if (GetWorld()->GetTimerManager().IsTimerActive(WeaponSwitchTimer))
    {
        UE_LOG(LogTemp, Warning, TEXT("Already switching weapons, ignoring request for %s"), *WeaponClass->GetName());
        return;
    }

    PendingWeapon = WeaponToEquip;
    float SwitchTime = 0.5f; // Default
    if (CurrentWeapon && CurrentWeapon->GetWeaponData()) SwitchTime = CurrentWeapon->GetWeaponData()->WeaponStats.WeaponSwitchTime;
    else if (PendingWeapon && PendingWeapon->GetWeaponData()) SwitchTime = PendingWeapon->GetWeaponData()->WeaponStats.WeaponSwitchTime; // Switch time for weapon being equipped

    UE_LOG(LogTemp, Warning, TEXT("Starting weapon switch to %s, time: %f"), *PendingWeapon->GetName(), SwitchTime);

    GetWorld()->GetTimerManager().SetTimer(WeaponSwitchTimer, this, &UWeaponInventoryComponent::FinishWeaponSwitch, SwitchTime, false);

    if (CurrentWeapon)
    {
        // Character's OnWeaponEquipped(nullptr) during the switch process will clear old abilities.
        // For now, just unequip visually. The new abilities are granted on FinishWeaponSwitch.
        CurrentWeapon->Unequip();
        // Let character know current weapon is going away before new one is ready
        AStrafeCharacter* Character = Cast<AStrafeCharacter>(GetOwner());
        if (Character) Character->OnWeaponEquipped(nullptr);
    }
}

void UWeaponInventoryComponent::ServerEquipWeapon_Implementation(TSubclassOf<ABaseWeapon> WeaponClass)
{
    EquipWeapon(WeaponClass);
}

void UWeaponInventoryComponent::FinishWeaponSwitch()
{
    if (PendingWeapon)
    {
        UE_LOG(LogTemp, Warning, TEXT("Finishing weapon switch to: %s"), *PendingWeapon->GetName());
        if (CurrentWeapon && CurrentWeapon != PendingWeapon) // Ensure old weapon is unequipped if it wasn't part of initial timer call
        {
            // This might be redundant if EquipWeapon(nullptr) was called before timer
            // CurrentWeapon->Unequip();
        }
        CurrentWeapon = PendingWeapon;
        PendingWeapon = nullptr;

        ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
        if (OwnerCharacter && CurrentWeapon)
        {
            CurrentWeapon->Equip(OwnerCharacter);
        }

        OnRep_CurrentWeapon(); // Replicate the new CurrentWeapon
        OnWeaponEquipped.Broadcast(CurrentWeapon); // Notify server-side systems and Character

        // MulticastEquipWeaponVisuals(CurrentWeapon); // This is now implicitly handled by OnRep_CurrentWeapon for visuals
                                                  // and OnWeaponEquipped on Character for abilities
    }
}

void UWeaponInventoryComponent::MulticastEquipWeaponVisuals_Implementation(ABaseWeapon* NewWeapon)
{
    // This function is less critical now.
    // OnRep_CurrentWeapon handles replicating the CurrentWeapon pointer.
    // AStrafeCharacter::OnWeaponEquipped (called due to OnWeaponEquipped delegate or OnRep_CurrentWeapon)
    // should handle visual attachment if needed on clients, or the ABaseWeapon::Equip handles it.
    // For purely visual things that clients need to do immediately that replication doesn't cover, use this.
    // But ability granting is server-authoritative.

    // If CurrentWeapon was set locally on client due to prediction, this call could confirm/correct.
    // However, standard GAS relies on server granting abilities.
    if (GetOwnerRole() < ROLE_Authority) // Only clients execute this
    {
        // ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
        // if (OwnerCharacter && NewWeapon) {
        //    NewWeapon->Equip(OwnerCharacter); // Ensure visual equip on client
        // }
        // if (CurrentWeapon && CurrentWeapon != NewWeapon) {
        //    CurrentWeapon->Unequip();
        // }
        // CurrentWeapon = NewWeapon; // Update local client's notion
    }
    // OnWeaponEquipped.Broadcast(NewWeapon); // This was already broadcast on server
}


void UWeaponInventoryComponent::OnRep_CurrentWeapon()
{
    // This is called on clients when CurrentWeapon changes.
    // The AStrafeCharacter should observe its inventory's OnWeaponEquipped event,
    // or react to this OnRep_CurrentWeapon if direct coupling is preferred.
    // For simplicity, let's assume AStrafeCharacter's OnWeaponEquipped delegate handles it.
    OnWeaponEquipped.Broadcast(CurrentWeapon); // Notify local client systems

    // If there's any purely visual unequip/equip logic that needs to happen on clients
    // when the replicated CurrentWeapon changes, it can be done here.
    // For example, if a previously equipped weapon (not the one before CurrentWeapon, but one before that)
    // wasn't properly hidden due to prediction, this is a chance to correct it.
    // However, BaseWeapon::Equip and Unequip should handle visibility.
    for (ABaseWeapon* Weapon : WeaponInventory)
    {
        if (Weapon && Weapon != CurrentWeapon && Weapon->IsEquipped())
        {
            Weapon->Unequip(); // Ensure only current weapon is visually equipped
        }
    }
    if (CurrentWeapon && !CurrentWeapon->IsEquipped())
    {
        ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
        if (OwnerCharacter) CurrentWeapon->Equip(OwnerCharacter);
    }
}

void UWeaponInventoryComponent::OnRep_WeaponInventory()
{
    UE_LOG(LogTemp, Warning, TEXT("Client: Weapon inventory replicated, count: %d"), WeaponInventory.Num());
    // Potentially update UI or other client-side systems that care about the raw list.
}

// OnRep_AmmoReserves and ammo functions are removed.

void UWeaponInventoryComponent::NextWeapon()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return; // Should be called on server
    if (WeaponInventory.Num() <= 1) return;

    int32 CurrentIndex = CurrentWeapon ? WeaponInventory.IndexOfByKey(CurrentWeapon) : -1;
    int32 NextIndex = (CurrentIndex == INDEX_NONE) ? 0 : (CurrentIndex + 1) % WeaponInventory.Num();

    EquipWeaponByIndex(NextIndex);
}

void UWeaponInventoryComponent::PreviousWeapon()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return; // Should be called on server
    if (WeaponInventory.Num() <= 1) return;

    int32 CurrentIndex = CurrentWeapon ? WeaponInventory.IndexOfByKey(CurrentWeapon) : -1;
    int32 PrevIndex = (CurrentIndex == INDEX_NONE) ? 0 : (CurrentIndex - 1 + WeaponInventory.Num()) % WeaponInventory.Num();

    EquipWeaponByIndex(PrevIndex);
}

void UWeaponInventoryComponent::EquipWeaponByIndex(int32 Index)
{
    if (WeaponInventory.IsValidIndex(Index) && WeaponInventory[Index])
    {
        EquipWeapon(WeaponInventory[Index]->GetClass());
    }
}

bool UWeaponInventoryComponent::HasWeapon(TSubclassOf<ABaseWeapon> WeaponClass) const
{
    for (const ABaseWeapon* Weapon : WeaponInventory)
    {
        if (Weapon && Weapon->IsA(WeaponClass))
        {
            return true;
        }
    }
    return false;
}