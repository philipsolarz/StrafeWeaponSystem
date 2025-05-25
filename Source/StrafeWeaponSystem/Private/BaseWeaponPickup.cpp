// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseWeaponPickup.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "StrafeCharacter.h"
#include "WeaponInventoryComponent.h"
#include "BaseWeapon.h" // For WeaponClassToGrant
#include "WeaponDataAsset.h" // For accessing weapon data during pickup
#include "AbilitySystemComponent.h" // For applying GEs
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Engine/Engine.h" 

ABaseWeaponPickup::ABaseWeaponPickup()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
    RootComponent = CollisionSphere;
    CollisionSphere->SetSphereRadius(100.0f);
    CollisionSphere->SetCollisionProfileName("OverlapAllDynamic");

    PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
    PickupMesh->SetupAttachment(RootComponent);
    PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // WeaponClassToGrant = nullptr; // Defaulted in header
    // PickupGameplayEffect = nullptr; // Defaulted in header
    bDestroyOnPickup = true;
    RespawnTime = 30.0f;
    bIsActive = true;
}

void ABaseWeaponPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABaseWeaponPickup, bIsActive);
}

void ABaseWeaponPickup::BeginPlay()
{
    Super::BeginPlay();
    if (CollisionSphere)
    {
        CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &ABaseWeaponPickup::OnSphereBeginOverlap);
    }
    OnRep_IsActive(); // Ensure initial visual state

    if (WeaponClassToGrant) UE_LOG(LogTemp, Log, TEXT("WeaponPickup '%s' will grant weapon: %s"), *GetName(), *WeaponClassToGrant->GetName());
    if (PickupGameplayEffect) UE_LOG(LogTemp, Log, TEXT("WeaponPickup '%s' will apply GE: %s"), *GetName(), *PickupGameplayEffect->GetName());
    if (!WeaponClassToGrant && !PickupGameplayEffect) UE_LOG(LogTemp, Warning, TEXT("WeaponPickup '%s' has neither WeaponClassToGrant nor PickupGameplayEffect set!"), *GetName());
}

void ABaseWeaponPickup::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (HasAuthority() && bIsActive && OtherActor)
    {
        AStrafeCharacter* Character = Cast<AStrafeCharacter>(OtherActor);
        if (Character)
        {
            ProcessPickup(Character);
        }
    }
}

void ABaseWeaponPickup::ProcessPickup(AStrafeCharacter* PickingCharacter)
{
    if (!PickingCharacter) return;

    UAbilitySystemComponent* ASC = PickingCharacter->GetAbilitySystemComponent();
    UWeaponInventoryComponent* Inventory = PickingCharacter->GetWeaponInventoryComponent();

    if (!ASC || !Inventory)
    {
        UE_LOG(LogTemp, Error, TEXT("ProcessPickup: Character %s missing ASC or InventoryComponent."), *PickingCharacter->GetName());
        return;
    }

    bool bWeaponGrantedOrAlreadyHad = false;

    // If this pickup can grant a new weapon
    if (WeaponClassToGrant)
    {
        if (!Inventory->HasWeapon(WeaponClassToGrant))
        {
            UE_LOG(LogTemp, Log, TEXT("ProcessPickup: Attempting to add weapon %s to %s."), *WeaponClassToGrant->GetName(), *PickingCharacter->GetName());
            if (Inventory->AddWeapon(WeaponClassToGrant)) // AddWeapon now handles initial ammo GE
            {
                bWeaponGrantedOrAlreadyHad = true;
                // If this is the first weapon, equip it automatically
                if (!Inventory->GetCurrentWeapon())
                {
                    Inventory->EquipWeapon(WeaponClassToGrant);
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("ProcessPickup: Failed to add weapon %s to %s."), *WeaponClassToGrant->GetName(), *PickingCharacter->GetName());
            }
        }
        else // Already has the weapon
        {
            bWeaponGrantedOrAlreadyHad = true; // For the purpose of applying the GE
            UE_LOG(LogTemp, Log, TEXT("ProcessPickup: Character %s already has weapon %s."), *PickingCharacter->GetName(), *WeaponClassToGrant->GetName());
        }
    }

    // Apply the main GameplayEffect (for ammo, health, etc.)
    // This GE could be specifically for "ammo for the granted weapon" or a generic "ammo refill"
    if (PickupGameplayEffect)
    {
        FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
        ContextHandle.AddSourceObject(this); // Source of the GE is the pickup itself
        FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(PickupGameplayEffect, 1, ContextHandle);

        if (SpecHandle.IsValid())
        {
            ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
            UE_LOG(LogTemp, Log, TEXT("ProcessPickup: Applied GE %s to %s."), *PickupGameplayEffect->GetName(), *PickingCharacter->GetName());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("ProcessPickup: Failed to make spec for GE %s."), *PickupGameplayEffect->GetName());
        }
    }
    else if (WeaponClassToGrant && bWeaponGrantedOrAlreadyHad)
    {
        // If it was a weapon pickup that granted/topped-up a weapon, but had NO specific PickupGameplayEffect,
        // the AddWeapon method already handled initial ammo.
        UE_LOG(LogTemp, Log, TEXT("ProcessPickup: Weapon %s granted/topped. Initial ammo handled by AddWeapon. No additional PickupGameplayEffect on this pickup actor."), *WeaponClassToGrant->GetName());
    }


    Multicast_OnPickedUpEffects();

    if (bDestroyOnPickup)
    {
        Destroy();
    }
    else
    {
        SetPickupActiveState(false);
        GetWorldTimerManager().SetTimer(RespawnTimerHandle, this, &ABaseWeaponPickup::AttemptRespawn, RespawnTime, false);
    }
}

void ABaseWeaponPickup::Multicast_OnPickedUpEffects_Implementation()
{
    OnPickedUpEffectsBP();
}

void ABaseWeaponPickup::AttemptRespawn()
{
    if (HasAuthority())
    {
        SetPickupActiveState(true);
    }
}

void ABaseWeaponPickup::SetPickupActiveState(bool bNewActiveState)
{
    if (HasAuthority())
    {
        bIsActive = bNewActiveState;
        OnRep_IsActive();
    }
}

void ABaseWeaponPickup::OnRep_IsActive()
{
    SetActorHiddenInGame(!bIsActive);
    CollisionSphere->SetCollisionEnabled(bIsActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    if (PickupMesh) PickupMesh->SetVisibility(bIsActive);

    if (bIsActive) OnRespawnEffectsBP();
}

// void ABaseWeaponPickup::Tick(float DeltaTime)
// {
//     Super::Tick(DeltaTime);
// }