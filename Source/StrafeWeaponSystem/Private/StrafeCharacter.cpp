// Copyright Epic Games, Inc. All Rights Reserved.

#include "StrafeCharacter.h"
#include "WeaponInventoryComponent.h"
#include "BaseWeapon.h"
#include "WeaponDataAsset.h" 
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h" 
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "AbilitySystemComponent.h" 
#include "StrafeAttributeSet.h"   
#include "GameplayAbilitySpec.h" 
#include "GameplayEffectTypes.h" 
#include "GA_WeaponActivate.h" // Required for AbilityCDO


// Sets default values
AStrafeCharacter::AStrafeCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	WeaponInventoryComponent = CreateDefaultSubobject<UWeaponInventoryComponent>(TEXT("WeaponInventoryComponent"));

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UStrafeAttributeSet>(TEXT("AttributeSet"));

	CurrentPrimaryFireInputID = -1;
	CurrentSecondaryFireInputID = -1;
}

UAbilitySystemComponent* AStrafeCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AStrafeCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (WeaponInventoryComponent)
	{
		WeaponInventoryComponent->OnWeaponEquipped.AddDynamic(this, &AStrafeCharacter::OnWeaponEquipped);
	}

	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			if (MovementMappingContext)
			{
				Subsystem->AddMappingContext(MovementMappingContext, 0);
			}
			if (WeaponMappingContext)
			{
				Subsystem->AddMappingContext(WeaponMappingContext, 1);
			}
		}
	}
	if (HasAuthority())
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

void AStrafeCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		InitializeAttributes();
		GiveDefaultAbilities();
	}
}

void AStrafeCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}


void AStrafeCharacter::InitializeAttributes()
{
	if (!AbilitySystemComponent || !AttributeSet) return;

	if (HasAuthority())
	{
		if (DefaultAttributesEffect)
		{
			FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
			EffectContext.AddSourceObject(this);
			FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(DefaultAttributesEffect, 1, EffectContext);
			if (SpecHandle.IsValid())
			{
				AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("AStrafeCharacter::InitializeAttributes: No DefaultAttributesEffect set. Consider creating one to initialize attributes."));
		}
	}
}

void AStrafeCharacter::GiveDefaultAbilities()
{
	if (!AbilitySystemComponent || !HasAuthority()) return;

	for (TSubclassOf<UGameplayAbility> AbilityClass : DefaultAbilities)
	{
		if (AbilityClass)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
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
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ACharacter::Jump);
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		}

		// Primary Fire
		if (PrimaryFireAction)
		{
			EnhancedInputComponent->BindAction(PrimaryFireAction, ETriggerEvent::Triggered, this, &AStrafeCharacter::Input_PrimaryFire_Pressed);
			EnhancedInputComponent->BindAction(PrimaryFireAction, ETriggerEvent::Completed, this, &AStrafeCharacter::Input_PrimaryFire_Released);
		}

		// Secondary Fire
		if (SecondaryFireAction)
		{
			EnhancedInputComponent->BindAction(SecondaryFireAction, ETriggerEvent::Triggered, this, &AStrafeCharacter::Input_SecondaryFire_Pressed);
			EnhancedInputComponent->BindAction(SecondaryFireAction, ETriggerEvent::Completed, this, &AStrafeCharacter::Input_SecondaryFire_Released);
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
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AStrafeCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	if (Controller != nullptr)
	{
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void AStrafeCharacter::Input_PrimaryFire_Pressed()
{
	UE_LOG(LogTemp, Warning, TEXT("AStrafeCharacter::Input_PrimaryFire_Pressed - InputID: %d, ASC: %s"),
		CurrentPrimaryFireInputID,
		AbilitySystemComponent ? TEXT("Valid") : TEXT("Null"));

	if (AbilitySystemComponent && CurrentPrimaryFireInputID != -1)
	{
		AbilitySystemComponent->AbilityLocalInputPressed(CurrentPrimaryFireInputID);
	}
}

void AStrafeCharacter::Input_PrimaryFire_Released()
{
	UE_LOG(LogTemp, Warning, TEXT("AStrafeCharacter::Input_PrimaryFire_Released - InputID: %d, ASC: %s"),
		CurrentPrimaryFireInputID,
		AbilitySystemComponent ? TEXT("Valid") : TEXT("Null"));
	if (AbilitySystemComponent && CurrentPrimaryFireInputID != -1)
	{
		AbilitySystemComponent->AbilityLocalInputReleased(CurrentPrimaryFireInputID);
	}
}

void AStrafeCharacter::Input_SecondaryFire_Pressed()
{
	UE_LOG(LogTemp, Warning, TEXT("AStrafeCharacter::Input_SecondaryFire_Pressed - InputID: %d, ASC: %s"),
		CurrentSecondaryFireInputID,
		AbilitySystemComponent ? TEXT("Valid") : TEXT("Null"));
	if (AbilitySystemComponent && CurrentSecondaryFireInputID != -1)
	{
		AbilitySystemComponent->AbilityLocalInputPressed(CurrentSecondaryFireInputID);
	}
}

void AStrafeCharacter::Input_SecondaryFire_Released()
{
	UE_LOG(LogTemp, Warning, TEXT("AStrafeCharacter::Input_SecondaryFire_Released - InputID: %d, ASC: %s"),
		CurrentSecondaryFireInputID,
		AbilitySystemComponent ? TEXT("Valid") : TEXT("Null"));
	if (AbilitySystemComponent && CurrentSecondaryFireInputID != -1)
	{
		AbilitySystemComponent->AbilityLocalInputReleased(CurrentSecondaryFireInputID);
	}
}

void AStrafeCharacter::OnWeaponEquipped(ABaseWeapon* NewWeapon)
{
	UE_LOG(LogTemp, Warning, TEXT("AStrafeCharacter::OnWeaponEquipped called. NewWeapon: %s"), NewWeapon ? *NewWeapon->GetName() : TEXT("nullptr"));

	if (!AbilitySystemComponent) // Removed HasAuthority() check here, clients also need to update InputIDs
	{
		UE_LOG(LogTemp, Warning, TEXT("AStrafeCharacter::OnWeaponEquipped - Early exit. ASC: %s"),
			AbilitySystemComponent ? TEXT("Valid") : TEXT("Null"));
		return;
	}

	// Clear old abilities only if authoritative, and update input IDs for all
	if (HasAuthority())
	{
		UE_LOG(LogTemp, Log, TEXT("AStrafeCharacter::OnWeaponEquipped (Authority) - Clearing %d previous weapon abilities"), CurrentWeaponAbilityHandles.Num());
		for (FGameplayAbilitySpecHandle Handle : CurrentWeaponAbilityHandles)
		{
			AbilitySystemComponent->ClearAbility(Handle);
		}
		CurrentWeaponAbilityHandles.Empty();
	}

	// Reset current input IDs
	CurrentPrimaryFireInputID = -1;
	CurrentSecondaryFireInputID = -1;

	if (NewWeapon && NewWeapon->GetWeaponData())
	{
		UWeaponDataAsset* WeaponData = NewWeapon->GetWeaponData();
		UE_LOG(LogTemp, Log, TEXT("AStrafeCharacter::OnWeaponEquipped - Processing WeaponData: %s"), *WeaponData->GetName());

		// Grant Primary Fire Ability
		if (WeaponData->PrimaryFireAbility)
		{
			UE_LOG(LogTemp, Log, TEXT("AStrafeCharacter::OnWeaponEquipped - PrimaryFireAbility class: %s"),
				*WeaponData->PrimaryFireAbility->GetName());

			if (!WeaponData->PrimaryFireAbility->IsChildOf(UGA_WeaponActivate::StaticClass()))
			{
				UE_LOG(LogTemp, Error, TEXT("AStrafeCharacter::OnWeaponEquipped - PrimaryFireAbility %s is not a child of UGA_WeaponActivate!"),
					*WeaponData->PrimaryFireAbility->GetName());
			}
			else
			{
				UGA_WeaponActivate* AbilityCDO = WeaponData->PrimaryFireAbility->GetDefaultObject<UGA_WeaponActivate>();
				if (AbilityCDO)
				{
					CurrentPrimaryFireInputID = AbilityCDO->AbilityInputID; // Store InputID
					if (HasAuthority()) // Grant ability only on server
					{
						FGameplayAbilitySpecHandle SpecHandle = AbilitySystemComponent->GiveAbility(
							FGameplayAbilitySpec(WeaponData->PrimaryFireAbility, 1, AbilityCDO->AbilityInputID, this)
						);
						CurrentWeaponAbilityHandles.Add(SpecHandle);
						UE_LOG(LogTemp, Log, TEXT("AStrafeCharacter::OnWeaponEquipped (Authority) - Granted primary ability with InputID: %d"), AbilityCDO->AbilityInputID);
					}
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("AStrafeCharacter::OnWeaponEquipped - Failed to get CDO for PrimaryFireAbility"));
				}
			}
		}

		// Grant Secondary Fire Ability
		if (WeaponData->SecondaryFireAbility)
		{
			UE_LOG(LogTemp, Log, TEXT("AStrafeCharacter::OnWeaponEquipped - SecondaryFireAbility class: %s"),
				*WeaponData->SecondaryFireAbility->GetName());

			if (!WeaponData->SecondaryFireAbility->IsChildOf(UGA_WeaponActivate::StaticClass()))
			{
				UE_LOG(LogTemp, Error, TEXT("AStrafeCharacter::OnWeaponEquipped - SecondaryFireAbility %s is not a child of UGA_WeaponActivate!"),
					*WeaponData->SecondaryFireAbility->GetName());
			}
			else
			{
				UGA_WeaponActivate* AbilityCDO = WeaponData->SecondaryFireAbility->GetDefaultObject<UGA_WeaponActivate>();
				if (AbilityCDO)
				{
					CurrentSecondaryFireInputID = AbilityCDO->AbilityInputID; // Store InputID
					if (HasAuthority()) // Grant ability only on server
					{
						FGameplayAbilitySpecHandle SpecHandle = AbilitySystemComponent->GiveAbility(
							FGameplayAbilitySpec(WeaponData->SecondaryFireAbility, 1, AbilityCDO->AbilityInputID, this)
						);
						CurrentWeaponAbilityHandles.Add(SpecHandle);
						UE_LOG(LogTemp, Log, TEXT("AStrafeCharacter::OnWeaponEquipped (Authority) - Granted secondary ability with InputID: %d"), AbilityCDO->AbilityInputID);
					}
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("AStrafeCharacter::OnWeaponEquipped - Failed to get CDO for SecondaryFireAbility"));
				}
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AStrafeCharacter::OnWeaponEquipped - No weapon or weapon data"));
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