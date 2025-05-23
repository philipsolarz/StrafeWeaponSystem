#include "WeaponInventoryComponent.h"
#include "BaseWeapon.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "Engine/ActorChannel.h"

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
            AddWeapon(WeaponClass);
        }

        // Equip first weapon if available
        if (WeaponInventory.Num() > 0)
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
    if (!GetOwner()->HasAuthority())
    {
        ServerAddWeapon(WeaponClass);
        return false;
    }

    // Check if we already have this weapon type
    for (ABaseWeapon* Weapon : WeaponInventory)
    {
        if (Weapon && Weapon->IsA(WeaponClass))
        {
            // Just add ammo to existing weapon
            if (UWeaponDataAsset* WeaponData = Weapon->GetWeaponData())
            {
                Weapon->AddAmmo(WeaponData->WeaponStats.AmmoPerPickup);
            }
            return false;
        }
    }

    // Spawn new weapon
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = GetOwner();
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ABaseWeapon* NewWeapon = GetWorld()->SpawnActor<ABaseWeapon>(WeaponClass, SpawnParams);
    if (NewWeapon)
    {
        WeaponInventory.Add(NewWeapon);
        NewWeapon->SetOwner(GetOwner());
        NewWeapon->AttachToComponent(GetOwner()->GetRootComponent(),
            FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        NewWeapon->SetActorHiddenInGame(true);

        OnWeaponAdded.Broadcast(WeaponClass);
        return true;
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

    for (ABaseWeapon* Weapon : WeaponInventory)
    {
        if (Weapon && Weapon->IsA(WeaponClass))
        {
            // Check if we're already switching
            if (GetWorld()->GetTimerManager().IsTimerActive(WeaponSwitchTimer))
                return;

            PendingWeapon = Weapon;

            // Calculate switch time
            float SwitchTime = 0.5f;
            if (CurrentWeapon && CurrentWeapon->GetWeaponData())
            {
                SwitchTime = CurrentWeapon->GetWeaponData()->WeaponStats.WeaponSwitchTime;
            }

            // Start switch timer
            GetWorld()->GetTimerManager().SetTimer(
                WeaponSwitchTimer,
                this,
                &UWeaponInventoryComponent::FinishWeaponSwitch,
                SwitchTime,
                false
            );

            // Hide current weapon
            if (CurrentWeapon)
            {
                CurrentWeapon->Unequip();
            }

            break;
        }
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
        CurrentWeapon = PendingWeapon;
        PendingWeapon = nullptr;

        if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
        {
            CurrentWeapon->Equip(Character);
        }

        MulticastEquipWeapon(CurrentWeapon);
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
    int32 NextIndex = (CurrentIndex + 1) % WeaponInventory.Num();

    EquipWeaponByIndex(NextIndex);
}

void UWeaponInventoryComponent::PreviousWeapon()
{
    if (WeaponInventory.Num() <= 1)
        return;

    int32 CurrentIndex = WeaponInventory.IndexOfByKey(CurrentWeapon);
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