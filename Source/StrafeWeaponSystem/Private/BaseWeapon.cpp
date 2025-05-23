#include "BaseWeapon.h"
#include "WeaponInventoryComponent.h"
#include "ProjectileBase.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

ABaseWeapon::ABaseWeapon()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
    RootComponent = WeaponMesh;
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
    if (!WeaponData || !WeaponData->WeaponStats.PrimaryProjectileClass)
        return;

    // Check for max active projectiles
    if (WeaponData->WeaponStats.MaxActiveProjectiles > 0 &&
        ActiveProjectiles.Num() >= WeaponData->WeaponStats.MaxActiveProjectiles)
    {
        return;
    }

    if (!ConsumeAmmo(1))
    {
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

        FVector MuzzleLocation = WeaponMesh->GetSocketLocation("Muzzle");
        FRotator MuzzleRotation = WeaponMesh->GetSocketRotation("Muzzle");

        if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
        {
            MuzzleRotation = Character->GetControlRotation();
        }

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
        }
    }

    OnWeaponFired.Broadcast();
    OnPrimaryFire();
    MulticastFireEffects();
}

void ABaseWeapon::MulticastFireEffects_Implementation()
{
    // This is called on all clients for visual/audio effects
    // Blueprint can implement the actual VFX/SFX logic
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
        CurrentAmmo = FMath::Clamp(CurrentAmmo + Amount, 0, WeaponData->WeaponStats.MaxAmmo);
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
    // Base implementation does nothing
    // Derived classes override this for specific behavior
    OnSecondaryFire();
}

void ABaseWeapon::Equip(ACharacter* NewOwner)
{
    if (!NewOwner)
        return;

    SetOwner(NewOwner);
    bIsEquipped = true;
    SetActorHiddenInGame(false);

    // Attach to character
    if (USkeletalMeshComponent* CharacterMesh = NewOwner->GetMesh())
    {
        AttachToComponent(CharacterMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, "WeaponSocket");
    }
}

void ABaseWeapon::Unequip()
{
    bIsEquipped = false;
    SetActorHiddenInGame(true);

    // Stop any ongoing fire
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
}

void ABaseWeapon::UnregisterProjectile(AProjectileBase* Projectile)
{
}
