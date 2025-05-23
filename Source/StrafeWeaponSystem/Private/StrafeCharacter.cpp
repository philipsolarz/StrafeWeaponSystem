// Copyright Epic Games, Inc. All Rights Reserved.

#include "StrafeCharacter.h"
#include "WeaponInventoryComponent.h"
#include "BaseWeapon.h" // Include BaseWeapon.h
#include "Camera/CameraComponent.h" // For potential future camera setup
#include "Components/CapsuleComponent.h" // For potential future setup
#include "GameFramework/SpringArmComponent.h" // For potential future camera setup

// Sets default values
AStrafeCharacter::AStrafeCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    WeaponInventoryComponent = CreateDefaultSubobject<UWeaponInventoryComponent>(TEXT("WeaponInventoryComponent"));

    // Example: If you want to setup a basic first-person camera:
    // Create a CameraComponent
    // UCameraComponent* FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    // FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
    // FirstPersonCameraComponent->SetRelativeLocation(FVector(-39.56f, 1.75f, 64.f)); // Position the camera
    // FirstPersonCameraComponent->bUsePawnControlRotation = true;
}

void AStrafeCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority()) // Server-side logic
    {
        if (WeaponInventoryComponent && StartingWeaponClass)
        {
            if (WeaponInventoryComponent->AddWeapon(StartingWeaponClass))
            {
                WeaponInventoryComponent->EquipWeapon(StartingWeaponClass);
            }
        }
    }
}

void AStrafeCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void AStrafeCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // Here you would bind your Enhanced Input Actions
    // Example (pseudo-code, actual binding depends on your Enhanced Input setup):
    // UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    // if (EnhancedInputComponent)
    // {
    //     EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &AStrafeCharacter::StartPrimaryFire);
    //     EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Completed, this, &AStrafeCharacter::StopPrimaryFire);
    //     // ... etc.
    // }
}

void AStrafeCharacter::StartPrimaryFire()
{
    if (WeaponInventoryComponent)
    {
        ABaseWeapon* CurrentWeapon = WeaponInventoryComponent->GetCurrentWeapon();
        if (CurrentWeapon)
        {
            CurrentWeapon->StartPrimaryFire();
        }
    }
}

void AStrafeCharacter::StopPrimaryFire()
{
    if (WeaponInventoryComponent)
    {
        ABaseWeapon* CurrentWeapon = WeaponInventoryComponent->GetCurrentWeapon();
        if (CurrentWeapon)
        {
            CurrentWeapon->StopPrimaryFire();
        }
    }
}

void AStrafeCharacter::StartSecondaryFire()
{
    if (WeaponInventoryComponent)
    {
        ABaseWeapon* CurrentWeapon = WeaponInventoryComponent->GetCurrentWeapon();
        if (CurrentWeapon)
        {
            CurrentWeapon->StartSecondaryFire();
        }
    }
}

void AStrafeCharacter::StopSecondaryFire()
{
    if (WeaponInventoryComponent)
    {
        ABaseWeapon* CurrentWeapon = WeaponInventoryComponent->GetCurrentWeapon();
        if (CurrentWeapon)
        {
            CurrentWeapon->StopSecondaryFire();
        }
    }
}

void AStrafeCharacter::NextWeapon()
{
    if (WeaponInventoryComponent)
    {
        WeaponInventoryComponent->NextWeapon();
    }
}

void AStrafeCharacter::PreviousWeapon()
{
    if (WeaponInventoryComponent)
    {
        WeaponInventoryComponent->PreviousWeapon();
    }
}

ABaseWeapon* AStrafeCharacter::GetCurrentWeapon() const
{
    if (WeaponInventoryComponent)
    {
        return WeaponInventoryComponent->GetCurrentWeapon();
    }
    return nullptr;
}