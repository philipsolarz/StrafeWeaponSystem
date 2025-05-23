// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseWeaponPickup.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "StrafeCharacter.h"
#include "WeaponInventoryComponent.h"
#include "BaseWeapon.h"
#include "Net/UnrealNetwork.h" // Required for DOREPLIFETIME
#include "TimerManager.h"

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

    WeaponClassToGrant = nullptr;
    bDestroyOnPickup = true;
    RespawnTime = 30.0f;
    bIsActive = true; // Start active
}

void ABaseWeaponPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABaseWeaponPickup, bIsActive);
}

void ABaseWeaponPickup::BeginPlay()
{
    Super::BeginPlay();
    // Ensure initial state is correctly set visually, especially for clients if spawned after server sets bIsActive
    OnRep_IsActive();
}

void ABaseWeaponPickup::NotifyActorBeginOverlap(AActor* OtherActor)
{
    Super::NotifyActorBeginOverlap(OtherActor);

    if (HasAuthority() && bIsActive)
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
    if (!PickingCharacter || !WeaponClassToGrant)
    {
        return;
    }

    UWeaponInventoryComponent* Inventory = PickingCharacter->FindComponentByClass<UWeaponInventoryComponent>();
    if (Inventory)
    {
        Inventory->AddWeapon(WeaponClassToGrant); // AddWeapon handles giving ammo if already possessed

        Multicast_OnPickedUpEffects(); // Replicated effects

        if (bDestroyOnPickup)
        {
            Destroy();
        }
        else
        {
            SetPickupActiveState(false); // Server deactivates
            GetWorldTimerManager().SetTimer(RespawnTimerHandle, this, &ABaseWeaponPickup::AttemptRespawn, RespawnTime, false);
        }
    }
}

void ABaseWeaponPickup::Multicast_OnPickedUpEffects_Implementation()
{
    // This runs on server and all clients
    OnPickedUpEffectsBP(); // Call Blueprint implementable event

    // If you had C++ defined effects:
    // UGameplayStatics::PlaySoundAtLocation(this, PickupSound, GetActorLocation());
    // UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), PickupEffect, GetActorTransform());
}


void ABaseWeaponPickup::AttemptRespawn()
{
    if (HasAuthority())
    {
        SetPickupActiveState(true); // Server reactivates
    }
}

void ABaseWeaponPickup::SetPickupActiveState(bool bNewActiveState)
{
    if (HasAuthority())
    {
        bIsActive = bNewActiveState;
        OnRep_IsActive(); // Call RepNotify locally on server too for immediate visual update
    }
}

void ABaseWeaponPickup::OnRep_IsActive()
{
    SetActorHiddenInGame(!bIsActive);
    CollisionSphere->SetCollisionEnabled(bIsActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    PickupMesh->SetVisibility(bIsActive); // Ensure mesh visibility matches

    if (bIsActive)
    {
        OnRespawnEffectsBP(); // Call BP event for respawn effects
    }
    else
    {
        // Optionally, play a "deactivated" effect here if needed
    }
}

void ABaseWeaponPickup::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}