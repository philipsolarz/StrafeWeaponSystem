#include "BaseWeapon.h"
#include "WeaponInventoryComponent.h"
#include "ProjectileBase.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/Engine.h" // For debug messages
#include "DrawDebugHelpers.h" // For debug visualization
#include "Kismet/GameplayStatics.h" // For sound and effect spawning
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

ABaseWeapon::ABaseWeapon()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
    RootComponent = WeaponMesh;

    // Set collision to ignore all by default
    WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABaseWeapon::BeginPlay()
{
    Super::BeginPlay();

    if (WeaponData)
    {
        CurrentAmmo = WeaponData->WeaponStats.AmmoPerPickup;

        if (WeaponData->WeaponMesh)
        {
            WeaponMesh->SetSkeletalMesh(WeaponData->WeaponMesh);
        }

        UE_LOG(LogTemp, Warning, TEXT("Weapon %s initialized with %d ammo"),
            *GetName(), CurrentAmmo);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Weapon %s has no WeaponData assigned!"), *GetName());
    }
}

void ABaseWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ABaseWeapon, CurrentAmmo);
    DOREPLIFETIME(ABaseWeapon, bIsEquipped);
}

void ABaseWeapon::StartPrimaryFire()
{
    if (!HasAuthority())
    {
        ServerStartPrimaryFire();
    }

    if (GetWorld()->GetTimerManager().IsTimerActive(PrimaryFireTimer))
        return;

    PrimaryFireInternal();

    if (WeaponData && WeaponData->WeaponStats.PrimaryFireRate > 0.0f)
    {
        GetWorld()->GetTimerManager().SetTimer(
            PrimaryFireTimer,
            this,
            &ABaseWeapon::PrimaryFireInternal,
            WeaponData->WeaponStats.PrimaryFireRate,
            true
        );
    }
}

void ABaseWeapon::StopPrimaryFire()
{
    if (!HasAuthority())
    {
        ServerStopPrimaryFire();
    }

    GetWorld()->GetTimerManager().ClearTimer(PrimaryFireTimer);
}

void ABaseWeapon::ServerStartPrimaryFire_Implementation()
{
    StartPrimaryFire();
}

void ABaseWeapon::ServerStopPrimaryFire_Implementation()
{
    StopPrimaryFire();
}

void ABaseWeapon::PrimaryFireInternal()
{
    // Safety check: Don't fire if not equipped
    if (!bIsEquipped)
    {
        StopPrimaryFire();
        return;
    }

    if (!WeaponData || !WeaponData->WeaponStats.PrimaryProjectileClass)
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot fire - no WeaponData or ProjectileClass"));
        return;
    }

    // Check for max active projectiles
    if (WeaponData->WeaponStats.MaxActiveProjectiles > 0 &&
        ActiveProjectiles.Num() >= WeaponData->WeaponStats.MaxActiveProjectiles)
    {
        UE_LOG(LogTemp, Warning, TEXT("Max active projectiles reached"));
        return;
    }

    if (!ConsumeAmmo(1))
    {
        UE_LOG(LogTemp, Warning, TEXT("Out of ammo"));
        OnOutOfAmmo.Broadcast();
        MulticastFireEffects(); // Play empty sound
        return;
    }

    // Spawn projectile
    if (HasAuthority())
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = GetOwner();
        SpawnParams.Instigator = Cast<APawn>(GetOwner());
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        // Get spawn transform with validation - UPDATED TO USE WEAPONDATA SOCKET NAME
        FVector MuzzleLocation = GetActorLocation() + GetActorForwardVector() * 100.0f; // Default offset
        FRotator MuzzleRotation = GetActorRotation();

        // Try to use muzzle socket if available - NOW USES WeaponData->MuzzleFlashSocketName
        if (WeaponMesh && WeaponData && WeaponMesh->DoesSocketExist(WeaponData->MuzzleFlashSocketName))
        {
            MuzzleLocation = WeaponMesh->GetSocketLocation(WeaponData->MuzzleFlashSocketName);
            MuzzleRotation = WeaponMesh->GetSocketRotation(WeaponData->MuzzleFlashSocketName);
        }

        // Use character's aim direction
        if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
        {
            if (AController* Controller = Character->GetController())
            {
                MuzzleRotation = Controller->GetControlRotation();

                // Trace from camera to find aim point
                FVector CameraLocation = Character->GetActorLocation() + FVector(0, 0, Character->BaseEyeHeight);
                FVector AimDirection = MuzzleRotation.Vector();
                FVector TraceEnd = CameraLocation + (AimDirection * 10000.0f);

                FHitResult HitResult;
                FCollisionQueryParams QueryParams;
                QueryParams.AddIgnoredActor(this);
                QueryParams.AddIgnoredActor(GetOwner());

                bool bHit = GetWorld()->LineTraceSingleByChannel(
                    HitResult,
                    CameraLocation,
                    TraceEnd,
                    ECC_Visibility,
                    QueryParams
                );

                // Adjust spawn location to be in front of camera but aim at hit point
                MuzzleLocation = CameraLocation + AimDirection * 150.0f;

                if (bHit)
                {
                    // Aim at hit point
                    FVector ToTarget = HitResult.Location - MuzzleLocation;
                    MuzzleRotation = ToTarget.Rotation();
                }

                // Debug visualization
#if WITH_EDITOR
                if (GetWorld()->GetNetMode() != NM_DedicatedServer)
                {
                    DrawDebugLine(GetWorld(), MuzzleLocation, MuzzleLocation + MuzzleRotation.Vector() * 500.0f,
                        FColor::Red, false, 1.0f, 0, 2.0f);
                }
#endif
            }
        }

        UE_LOG(LogTemp, Warning, TEXT("Spawning projectile at: %s, rot: %s"),
            *MuzzleLocation.ToString(), *MuzzleRotation.ToString());

        AProjectileBase* Projectile = GetWorld()->SpawnActor<AProjectileBase>(
            WeaponData->WeaponStats.PrimaryProjectileClass,
            MuzzleLocation,
            MuzzleRotation,
            SpawnParams
        );

        if (Projectile)
        {
            Projectile->InitializeProjectile(GetInstigatorController(), this);
            RegisterProjectile(Projectile);
            UE_LOG(LogTemp, Warning, TEXT("Projectile spawned successfully"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to spawn projectile"));
        }
    }

    OnWeaponFired.Broadcast();
    OnPrimaryFire();
    MulticastFireEffects();
}

void ABaseWeapon::MulticastFireEffects_Implementation()
{
    // UPDATED: Now implements actual effect spawning using DataAsset socket properties
    if (!WeaponData || !WeaponMesh)
        return;

    // Spawn muzzle flash effect at the correct socket
    if (WeaponData->MuzzleFlashEffect && WeaponMesh->DoesSocketExist(WeaponData->MuzzleFlashSocketName))
    {
        FVector EffectLocation = WeaponMesh->GetSocketLocation(WeaponData->MuzzleFlashSocketName);
        FRotator EffectRotation = WeaponMesh->GetSocketRotation(WeaponData->MuzzleFlashSocketName);

        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(),
            WeaponData->MuzzleFlashEffect,
            EffectLocation,
            EffectRotation
        );
    }

    // Play fire sound at the correct socket
    if (WeaponData->FireSound)
    {
        FVector SoundLocation = GetActorLocation(); // Default fallback

        if (WeaponMesh->DoesSocketExist(WeaponData->FireSoundSocketName))
        {
            SoundLocation = WeaponMesh->GetSocketLocation(WeaponData->FireSoundSocketName);
        }

        UGameplayStatics::PlaySoundAtLocation(
            GetWorld(),
            WeaponData->FireSound,
            SoundLocation
        );
    }

    // Call Blueprint implementable event for additional custom effects
    // This allows designers to add more effects in Blueprint while still having the C++ base functionality
}

bool ABaseWeapon::ConsumeAmmo(int32 Amount)
{
    if (CurrentAmmo >= Amount)
    {
        CurrentAmmo -= Amount;
        OnAmmoChanged.Broadcast(CurrentAmmo);
        return true;
    }
    return false;
}

void ABaseWeapon::AddAmmo(int32 Amount)
{
    if (WeaponData)
    {
        int32 OldAmmo = CurrentAmmo;
        CurrentAmmo = FMath::Clamp(CurrentAmmo + Amount, 0, WeaponData->WeaponStats.MaxAmmo);
        UE_LOG(LogTemp, Warning, TEXT("Added ammo: %d -> %d (max: %d)"),
            OldAmmo, CurrentAmmo, WeaponData->WeaponStats.MaxAmmo);
        OnAmmoChanged.Broadcast(CurrentAmmo);
    }
}

void ABaseWeapon::StartSecondaryFire()
{
    if (!HasAuthority())
    {
        ServerStartSecondaryFire();
    }

    if (!WeaponData || !WeaponData->WeaponStats.bHasSecondaryFire)
        return;

    if (GetWorld()->GetTimerManager().IsTimerActive(SecondaryFireTimer))
        return;

    SecondaryFireInternal();

    if (WeaponData->WeaponStats.SecondaryFireRate > 0.0f)
    {
        GetWorld()->GetTimerManager().SetTimer(
            SecondaryFireTimer,
            this,
            &ABaseWeapon::SecondaryFireInternal,
            WeaponData->WeaponStats.SecondaryFireRate,
            true
        );
    }
}

void ABaseWeapon::StopSecondaryFire()
{
    if (!HasAuthority())
    {
        ServerStopSecondaryFire();
    }

    GetWorld()->GetTimerManager().ClearTimer(SecondaryFireTimer);
}

void ABaseWeapon::ServerStartSecondaryFire_Implementation()
{
    StartSecondaryFire();
}

void ABaseWeapon::ServerStopSecondaryFire_Implementation()
{
    StopSecondaryFire();
}

void ABaseWeapon::SecondaryFireInternal()
{
    // Safety check: Don't fire if not equipped
    if (!bIsEquipped)
    {
        StopSecondaryFire();
        return;
    }

    // Base implementation does nothing
    // Derived classes override this for specific behavior
    OnSecondaryFire();
}

void ABaseWeapon::Equip(ACharacter* NewOwner)
{
    if (!NewOwner)
        return;

    UE_LOG(LogTemp, Warning, TEXT("Equipping weapon %s to %s"), *GetName(), *NewOwner->GetName());

    SetOwner(NewOwner);
    SetInstigator(NewOwner);
    bIsEquipped = true;
    SetActorHiddenInGame(false);

    // UPDATED: Play equip sound from WeaponData if available
    if (WeaponData && WeaponData->EquipSound)
    {
        UGameplayStatics::PlaySoundAtLocation(
            GetWorld(),
            WeaponData->EquipSound,
            GetActorLocation()
        );
    }

    // Attach to character
    if (USkeletalMeshComponent* CharacterMesh = NewOwner->GetMesh())
    {
        FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, true);
        AttachToComponent(CharacterMesh, AttachRules, "WeaponSocket");

        // Ensure proper transform
        SetActorRelativeLocation(FVector::ZeroVector);
        SetActorRelativeRotation(FRotator::ZeroRotator);
    }
}

void ABaseWeapon::Unequip()
{
    UE_LOG(LogTemp, Warning, TEXT("Unequipping weapon %s"), *GetName());

    // CRITICAL: Clear all firing timers first
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(PrimaryFireTimer);
        GetWorld()->GetTimerManager().ClearTimer(SecondaryFireTimer);
    }

    bIsEquipped = false;
    SetActorHiddenInGame(true);

    // Stop any ongoing fire (this is redundant but ensures cleanup)
    StopPrimaryFire();
    StopSecondaryFire();
}

int32 ABaseWeapon::GetMaxAmmo() const
{
    return WeaponData ? WeaponData->WeaponStats.MaxAmmo : 0;
}

void ABaseWeapon::OnRep_CurrentAmmo()
{
    OnAmmoChanged.Broadcast(CurrentAmmo);
}

void ABaseWeapon::RegisterProjectile(AProjectileBase* Projectile)
{
    if (Projectile && !ActiveProjectiles.Contains(Projectile))
    {
        ActiveProjectiles.Add(Projectile);

        // Set up delegate to remove projectile when destroyed
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