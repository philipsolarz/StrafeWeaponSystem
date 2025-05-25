#include "BaseWeapon.h"
#include "WeaponInventoryComponent.h" // May not be needed directly anymore
#include "ProjectileBase.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h" // Still used for equip/unequip logic if delayed
#include "Engine/Engine.h" // For debug messages
#include "DrawDebugHelpers.h" // For debug visualization
#include "Kismet/GameplayStatics.h" // For sound and effect spawning
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "WeaponDataAsset.h" // Required

ABaseWeapon::ABaseWeapon()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
    RootComponent = WeaponMesh;

    WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABaseWeapon::BeginPlay()
{
    Super::BeginPlay();

    // CurrentAmmo initialization is removed. Handled by AttributeSet.
    if (WeaponData)
    {
        if (WeaponData->WeaponMesh)
        {
            WeaponMesh->SetSkeletalMesh(WeaponData->WeaponMesh);
        }
        UE_LOG(LogTemp, Warning, TEXT("Weapon %s initialized."), *GetName());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Weapon %s has no WeaponData assigned!"), *GetName());
    }
}

void ABaseWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // DOREPLIFETIME(ABaseWeapon, CurrentAmmo); // Removed
    DOREPLIFETIME(ABaseWeapon, bIsEquipped);
    // DOREPLIFETIME(ABaseWeapon, StatModifierValues); // Removed
    // ActiveProjectiles are not typically replicated directly.
    // Their state/existence is usually handled by spawning them on server and replicating the actor itself.
}

// StartPrimaryFire, StopPrimaryFire, ServerStartPrimaryFire, ServerStopPrimaryFire, PrimaryFireInternal are REMOVED.
// MulticastFireEffects is REMOVED (effects handled by Ability or GameplayCue).
// ConsumeAmmo, AddAmmo, OnRep_CurrentAmmo are REMOVED.

// StartSecondaryFire, StopSecondaryFire, ServerStartSecondaryFire, ServerStopSecondaryFire, SecondaryFireInternal are REMOVED
// (or refactored if specific secondary actions like "detonate rockets" remain on the weapon actor callable by an ability)


void ABaseWeapon::Equip(ACharacter* NewOwner)
{
    if (!NewOwner)
        return;

    UE_LOG(LogTemp, Warning, TEXT("Equipping weapon %s to %s"), *GetName(), *NewOwner->GetName());

    SetOwner(NewOwner);
    SetInstigator(NewOwner); // Instigator is now the Character Pawn
    bIsEquipped = true;
    SetActorHiddenInGame(false);

    if (WeaponData && WeaponData->EquipSound)
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), WeaponData->EquipSound, GetActorLocation());
    }

    if (USkeletalMeshComponent* CharacterMesh = NewOwner->GetMesh())
    {
        FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, true);
        // Use AttachSocketName from WeaponData if available, otherwise the member variable
        FName SocketToAttach = (WeaponData && WeaponData->MuzzleFlashSocketName != NAME_None) ? WeaponData->MuzzleFlashSocketName : AttachSocketName; // This is likely an error, should be a dedicated attach socket from WeaponData or class
        // Corrected: Assume AttachSocketName is the intended socket for attaching to character.
        // If you want it per-weapon from DataAsset, add a new FName property there for it.
        AttachToComponent(CharacterMesh, AttachRules, AttachSocketName);

        SetActorRelativeLocation(FVector::ZeroVector);
        SetActorRelativeRotation(FRotator::ZeroRotator);
    }
}

void ABaseWeapon::Unequip()
{
    UE_LOG(LogTemp, Warning, TEXT("Unequipping weapon %s"), *GetName());

    // Timers for firing are gone.
    // If there are any weapon-specific active states (e.g. charging effects not tied to an ability), clear them.

    bIsEquipped = false;
    SetActorHiddenInGame(true);
    // SetOwner(nullptr); // Optional: clear owner if needed, but inventory component handles lifetime.
    // SetInstigator(nullptr);
}


void ABaseWeapon::RegisterProjectile(AProjectileBase* Projectile)
{
    if (Projectile && !ActiveProjectiles.Contains(Projectile))
    {
        ActiveProjectiles.Add(Projectile);
        Projectile->OnDestroyed.AddDynamic(this, &ABaseWeapon::OnProjectileDestroyed);
    }
}

void ABaseWeapon::UnregisterProjectile(AProjectileBase* Projectile)
{
    if (Projectile)
    {
        ActiveProjectiles.Remove(Projectile);
        Projectile->OnDestroyed.RemoveDynamic(this, &ABaseWeapon::OnProjectileDestroyed);
    }
}

void ABaseWeapon::OnProjectileDestroyed(AActor* DestroyedActor)
{
    if (AProjectileBase* Projectile = Cast<AProjectileBase>(DestroyedActor))
    {
        UnregisterProjectile(Projectile);
    }
}

// Modifier-aware stat getters are REMOVED.
// Base stats are in WeaponData. Modifiers are via GameplayEffects on the Character.
// Abilities will read these as needed.