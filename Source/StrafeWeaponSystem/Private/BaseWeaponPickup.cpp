// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseWeaponPickup.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "StrafeCharacter.h"
#include "WeaponInventoryComponent.h"
#include "BaseWeapon.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Engine/Engine.h" // For debug messages

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

    // Bind overlap events
    if (CollisionSphere)
    {
        CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &ABaseWeaponPickup::OnSphereBeginOverlap);
    }

    // Ensure initial state is correctly set visually
    OnRep_IsActive();

    // Debug: Log pickup initialization
    if (WeaponClassToGrant)
    {
        UE_LOG(LogTemp, Warning, TEXT("WeaponPickup '%s' initialized with weapon class: %s"),
            *GetName(), *WeaponClassToGrant->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("WeaponPickup '%s' has NO weapon class assigned!"), *GetName());
    }
}

void ABaseWeaponPickup::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // Debug log
    UE_LOG(LogTemp, Warning, TEXT("WeaponPickup overlap with: %s"), OtherActor ? *OtherActor->GetName() : TEXT("nullptr"));

    if (HasAuthority() && bIsActive && OtherActor)
    {
        AStrafeCharacter* Character = Cast<AStrafeCharacter>(OtherActor);
        if (Character)
        {
            ProcessPickup(Character);
        }
    }
}

void ABaseWeaponPickup::NotifyActorBeginOverlap(AActor* OtherActor)
{
    Super::NotifyActorBeginOverlap(OtherActor);

    // This is a backup in case the component overlap doesn't fire
    if (HasAuthority() && bIsActive && OtherActor)
    {
        AStrafeCharacter* Character = Cast<AStrafeCharacter>(OtherActor);
        if (Character)
        {
            UE_LOG(LogTemp, Warning, TEXT("WeaponPickup NotifyActorBeginOverlap fallback with: %s"),
                *Character->GetName());
            ProcessPickup(Character);
        }
    }
}

void ABaseWeaponPickup::ProcessPickup(AStrafeCharacter* PickingCharacter)
{
    if (!PickingCharacter || !WeaponClassToGrant)
    {
        UE_LOG(LogTemp, Error, TEXT("ProcessPickup failed - Character: %s, WeaponClass: %s"),
            PickingCharacter ? TEXT("Valid") : TEXT("nullptr"),
            WeaponClassToGrant ? TEXT("Valid") : TEXT("nullptr"));
        return;
    }

    // Get the inventory component directly from the character
    UWeaponInventoryComponent* Inventory = PickingCharacter->GetWeaponInventoryComponent();

    if (!Inventory)
    {
        UE_LOG(LogTemp, Error, TEXT("No WeaponInventoryComponent found on character: %s"),
            *PickingCharacter->GetName());
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Processing pickup - Adding weapon: %s to character: %s"),
        *WeaponClassToGrant->GetName(), *PickingCharacter->GetName());

    // Add the weapon
    bool bWeaponAdded = Inventory->AddWeapon(WeaponClassToGrant);

    // If this is the first weapon, equip it automatically
    if (bWeaponAdded && !Inventory->GetCurrentWeapon())
    {
        UE_LOG(LogTemp, Warning, TEXT("First weapon pickup - auto-equipping"));
        Inventory->EquipWeapon(WeaponClassToGrant);
    }

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