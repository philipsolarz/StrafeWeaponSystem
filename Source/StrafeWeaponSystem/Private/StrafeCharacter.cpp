// Copyright Epic Games, Inc. All Rights Reserved.

#include "StrafeCharacter.h"
#include "WeaponInventoryComponent.h"
#include "BaseWeapon.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h" // Required for movement component settings
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"

// Sets default values
AStrafeCharacter::AStrafeCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    WeaponInventoryComponent = CreateDefaultSubobject<UWeaponInventoryComponent>(TEXT("WeaponInventoryComponent"));

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

    // Add input mapping context
    if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
        {
            if (MovementMappingContext)
            {
                Subsystem->AddMappingContext(MovementMappingContext, 0); // Priority 0 for base movement
            }
            if (WeaponMappingContext)
            {
                Subsystem->AddMappingContext(WeaponMappingContext, 1); // Priority 1 for weapon actions
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

    if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
    {
        // Movement
        if (MoveAction)
        {
            EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AStrafeCharacter::Move);
        }

        if (LookAction)
        {
            EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AStrafeCharacter::Look);
        }

        if (JumpAction)
        {
            EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
            EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
        }

        // Primary Fire
        if (PrimaryFireAction)
        {
            EnhancedInputComponent->BindAction(PrimaryFireAction, ETriggerEvent::Started, this, &AStrafeCharacter::StartPrimaryFire);
            EnhancedInputComponent->BindAction(PrimaryFireAction, ETriggerEvent::Completed, this, &AStrafeCharacter::StopPrimaryFire);
        }

        // Secondary Fire
        if (SecondaryFireAction)
        {
            EnhancedInputComponent->BindAction(SecondaryFireAction, ETriggerEvent::Started, this, &AStrafeCharacter::StartSecondaryFire);
            EnhancedInputComponent->BindAction(SecondaryFireAction, ETriggerEvent::Completed, this, &AStrafeCharacter::StopSecondaryFire);
        }

        // Weapon Switching
        if (NextWeaponAction)
        {
            EnhancedInputComponent->BindAction(NextWeaponAction, ETriggerEvent::Triggered, this, &AStrafeCharacter::NextWeapon);
        }

        if (PreviousWeaponAction)
        {
            EnhancedInputComponent->BindAction(PreviousWeaponAction, ETriggerEvent::Triggered, this, &AStrafeCharacter::PreviousWeapon);
        }
    }
}

void AStrafeCharacter::Move(const FInputActionValue& Value)
{
    // input is a Vector2D
    FVector2D MovementVector = Value.Get<FVector2D>();

    if (Controller != nullptr)
    {
        // find out which way is forward
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);

        // get forward vector
        const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

        // get right vector
        const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

        // add movement
        AddMovementInput(ForwardDirection, MovementVector.Y);
        AddMovementInput(RightDirection, MovementVector.X);
    }
}

void AStrafeCharacter::Look(const FInputActionValue& Value)
{
    // input is a Vector2D
    FVector2D LookAxisVector = Value.Get<FVector2D>();

    if (Controller != nullptr)
    {
        // add yaw and pitch input to controller
        AddControllerYawInput(LookAxisVector.X);
        AddControllerPitchInput(LookAxisVector.Y);
    }
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