#include "WeaponInventoryComponent.h"
#include "BaseWeapon.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "Engine/ActorChannel.h"
#include "Engine/Engine.h" // For debug messages

UWeaponInventoryComponent::UWeaponInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UWeaponInventoryComponent::BeginPlay()
{
    Super::BeginPlay();

    if (GetOwner()->HasAuthority())
    {
        // Initialize ammo reserves
        AmmoReserves.Add(FAmmoReserve(EAmmoType::Rockets, 0));
        AmmoReserves.Add(FAmmoReserve(EAmmoType::StickyGrenades, 0));

        // Add starting weapons
        for (TSubclassOf<ABaseWeapon> WeaponClass : StartingWeapons)
        {
            if (WeaponClass)
            {
                AddWeapon(WeaponClass);
            }
        }

        // Equip first weapon if available
        if (WeaponInventory.Num() > 0 && WeaponInventory[0])
        {
            EquipWeapon(WeaponInventory[0]->GetClass());
        }
    }
}

void UWeaponInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UWeaponInventoryComponent, WeaponInventory);
    DOREPLIFETIME(UWeaponInventoryComponent, CurrentWeapon);
    DOREPLIFETIME(UWeaponInventoryComponent, AmmoReserves);
}

bool UWeaponInventoryComponent::AddWeapon(TSubclassOf<ABaseWeapon> WeaponClass)
{
    if (!WeaponClass)
    {
        UE_LOG(LogTemp, Error, TEXT("AddWeapon called with null WeaponClass"));
        return false;
    }

    if (!GetOwner()->HasAuthority())
    {
        ServerAddWeapon(WeaponClass);
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("AddWeapon called for: %s"), *WeaponClass->GetName());

    // Check if we already have this weapon type
    for (ABaseWeapon* Weapon : WeaponInventory)
    {
        if (Weapon && Weapon->IsA(WeaponClass))
        {
            // Just add ammo to existing weapon
            if (UWeaponDataAsset* WeaponData = Weapon->GetWeaponData())
            {
                int32 AmmoToAdd = WeaponData->WeaponStats.AmmoPerPickup;
                Weapon->AddAmmo(AmmoToAdd);
                UE_LOG(LogTemp, Warning, TEXT("Weapon already owned, added %d ammo"), AmmoToAdd);
            }
            return false;
        }
    }

    // Spawn new weapon
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = GetOwner();
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // Get spawn location away from world geometry
    FVector SpawnLocation = GetOwner()->GetActorLocation() + FVector(0, 0, 100);
    FRotator SpawnRotation = FRotator::ZeroRotator;

    ABaseWeapon* NewWeapon = GetWorld()->SpawnActor<ABaseWeapon>(WeaponClass, SpawnLocation, SpawnRotation, SpawnParams);
    if (NewWeapon)
    {
        UE_LOG(LogTemp, Warning, TEXT("Successfully spawned weapon: %s"), *NewWeapon->GetName());

        WeaponInventory.Add(NewWeapon);
        NewWeapon->SetOwner(GetOwner());

        // Attach to owner
        NewWeapon->AttachToComponent(GetOwner()->GetRootComponent(),
            FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        NewWeapon->SetActorHiddenInGame(true);

        OnWeaponAdded.Broadcast(WeaponClass);

        // Force replication update
        if (GetOwner()->HasAuthority())
        {
            OnRep_WeaponInventory();
        }

        return true;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to spawn weapon actor"));
    }

    return false;
}

void UWeaponInventoryComponent::ServerAddWeapon_Implementation(TSubclassOf<ABaseWeapon> WeaponClass)
{
    AddWeapon(WeaponClass);
}

void UWeaponInventoryComponent::EquipWeapon(TSubclassOf<ABaseWeapon> WeaponClass)
{
    if (!GetOwner()->HasAuthority())
    {
        ServerEquipWeapon(WeaponClass);
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("EquipWeapon called for: %s"),
        WeaponClass ? *WeaponClass->GetName() : TEXT("nullptr"));

    // Handle empty weapon class (unequip all)
    if (!WeaponClass)
    {
        if (CurrentWeapon)
        {
            // CRITICAL: Stop firing before unequipping
            CurrentWeapon->StopPrimaryFire();
            CurrentWeapon->StopSecondaryFire();
            CurrentWeapon->Unequip();
            CurrentWeapon = nullptr;
            OnRep_CurrentWeapon();
        }
        return;
    }

    // Find the weapon in inventory
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

    // Check if we're already switching
    if (GetWorld()->GetTimerManager().IsTimerActive(WeaponSwitchTimer))
    {
        UE_LOG(LogTemp, Warning, TEXT("Already switching weapons, ignoring request"));
        return;
    }

    PendingWeapon = WeaponToEquip;

    // Calculate switch time
    float SwitchTime = 0.5f;
    if (CurrentWeapon && CurrentWeapon->GetWeaponData())
    {
        SwitchTime = CurrentWeapon->GetWeaponData()->WeaponStats.WeaponSwitchTime;
    }

    UE_LOG(LogTemp, Warning, TEXT("Starting weapon switch, time: %f"), SwitchTime);

    // Start switch timer
    GetWorld()->GetTimerManager().SetTimer(
        WeaponSwitchTimer,
        this,
        &UWeaponInventoryComponent::FinishWeaponSwitch,
        SwitchTime,
        false
    );

    // CRITICAL: Stop firing and then unequip current weapon
    if (CurrentWeapon)
    {
        CurrentWeapon->StopPrimaryFire();
        CurrentWeapon->StopSecondaryFire();
        CurrentWeapon->Unequip();
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

        CurrentWeapon = PendingWeapon;
        PendingWeapon = nullptr;

        if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
        {
            CurrentWeapon->Equip(Character);
        }

        MulticastEquipWeapon(CurrentWeapon);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("FinishWeaponSwitch called but PendingWeapon is null"));
    }
}

void UWeaponInventoryComponent::MulticastEquipWeapon_Implementation(ABaseWeapon* NewWeapon)
{
    OnWeaponEquipped.Broadcast(NewWeapon);
}

void UWeaponInventoryComponent::OnRep_CurrentWeapon()
{
    OnWeaponEquipped.Broadcast(CurrentWeapon);
}

void UWeaponInventoryComponent::NextWeapon()
{
    if (WeaponInventory.Num() <= 1)
        return;

    int32 CurrentIndex = WeaponInventory.IndexOfByKey(CurrentWeapon);
    if (CurrentIndex == INDEX_NONE)
        CurrentIndex = 0;

    int32 NextIndex = (CurrentIndex + 1) % WeaponInventory.Num();

    EquipWeaponByIndex(NextIndex);
}

void UWeaponInventoryComponent::PreviousWeapon()
{
    if (WeaponInventory.Num() <= 1)
        return;

    int32 CurrentIndex = WeaponInventory.IndexOfByKey(CurrentWeapon);
    if (CurrentIndex == INDEX_NONE)
        CurrentIndex = 0;

    int32 PrevIndex = CurrentIndex - 1;
    if (PrevIndex < 0)
        PrevIndex = WeaponInventory.Num() - 1;

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

void UWeaponInventoryComponent::OnRep_WeaponInventory()
{
    // Notify UI or other systems about inventory changes
    UE_LOG(LogTemp, Warning, TEXT("Weapon inventory updated, count: %d"), WeaponInventory.Num());
}

void UWeaponInventoryComponent::AddAmmo(EAmmoType AmmoType, int32 Amount)
{
    int32 Index = FindAmmoReserveIndex(AmmoType);
    if (Index != INDEX_NONE)
    {
        AmmoReserves[Index].Count += Amount;
        OnRep_AmmoReserves(); // Trigger update on server
    }
}

int32 UWeaponInventoryComponent::GetAmmoCount(EAmmoType AmmoType) const
{
    int32 Index = FindAmmoReserveIndex(AmmoType);
    return (Index != INDEX_NONE) ? AmmoReserves[Index].Count : 0;
}

int32 UWeaponInventoryComponent::FindAmmoReserveIndex(EAmmoType AmmoType) const
{
    for (int32 i = 0; i < AmmoReserves.Num(); i++)
    {
        if (AmmoReserves[i].AmmoType == AmmoType)
        {
            return i;
        }
    }
    return INDEX_NONE;
}

void UWeaponInventoryComponent::OnRep_AmmoReserves()
{
    // Notify UI or other systems about ammo changes
}