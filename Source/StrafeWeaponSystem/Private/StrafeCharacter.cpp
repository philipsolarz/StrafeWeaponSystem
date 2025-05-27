// Copyright Epic Games, Inc. All Rights Reserved.

#include "StrafeCharacter.h"
#include "WeaponInventoryComponent.h"
#include "BaseWeapon.h"
#include "WeaponDataAsset.h" // Required for UWeaponDataAsset
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h" // Required for movement component settings
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "AbilitySystemComponent.h" // Required for UAbilitySystemComponent
#include "StrafeAttributeSet.h"   // Required for UStrafeAttributeSet
#include "GameplayAbilitySpec.h" //Required for FGameplayAbilitySpec
#include "GameplayEffectTypes.h" // Required for FGameplayEffectContextHandle
#include "GA_WeaponActivate.h"


// Sets default values
AStrafeCharacter::AStrafeCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	WeaponInventoryComponent = CreateDefaultSubobject<UWeaponInventoryComponent>(TEXT("WeaponInventoryComponent"));

	// Create Ability System Component
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed); // Or Minimal for more optimization

	// Create Attribute Set
	AttributeSet = CreateDefaultSubobject<UStrafeAttributeSet>(TEXT("AttributeSet"));

	// Set default input tags
	PrimaryFireInputTag = FGameplayTag::RequestGameplayTag(FName("Ability.Weapon.PrimaryFire"));
	SecondaryFireInputTag = FGameplayTag::RequestGameplayTag(FName("Ability.Weapon.SecondaryFire"));
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
		// Listen for weapon equip events from the inventory
		WeaponInventoryComponent->OnWeaponEquipped.AddDynamic(this, &AStrafeCharacter::OnWeaponEquipped);
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
	// Server-side: If starting weapon is defined, add and equip it
	// Note: ASC InitAbilityActorInfo will be called in PossessedBy/OnRep_PlayerState
	// Attribute and ability initialization will also happen there.
	if (HasAuthority())
	{
		if (WeaponInventoryComponent && StartingWeaponClass)
		{
			if (WeaponInventoryComponent->AddWeapon(StartingWeaponClass))
			{
				// Equipping will trigger OnWeaponEquipped, which handles ability granting
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
		AbilitySystemComponent->InitAbilityActorInfo(this, this); // Owner and Avatar are the Character itself
		InitializeAttributes();
		GiveDefaultAbilities();
	}
}

void AStrafeCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this); // Owner and Avatar are the Character itself
		// Attributes are already initialized by the server, clients just need to link up.
		// If you have client-only predicted attributes, initialize them here if not replicated.
		// For default abilities granted on spawn, clients might need to be granted them here too if not handled by initial replication.
		// However, abilities granted from weapon equipping should replicate fine.
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
			// Fallback: Manually initialize if no GE is set
			// Example:
			// AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetMaxRocketAmmoAttribute(), 20.f);
			// AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetRocketAmmoAttribute(), 10.f);
			// AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetMaxStickyGrenadeAmmoAttribute(), 8.f);
			// AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetStickyGrenadeAmmoAttribute(), 4.f);
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
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		}

		// Primary Fire
		if (PrimaryFireAction)
		{
			EnhancedInputComponent->BindAction(PrimaryFireAction, ETriggerEvent::Started, this, &AStrafeCharacter::Input_PrimaryFire_Pressed);
			// If your ability is instant or doesn't need a release:
			// EnhancedInputComponent->BindAction(PrimaryFireAction, ETriggerEvent::Completed, this, &AStrafeCharacter::Input_PrimaryFire_Released);
		}

		// Secondary Fire
		if (SecondaryFireAction)
		{
			EnhancedInputComponent->BindAction(SecondaryFireAction, ETriggerEvent::Started, this, &AStrafeCharacter::Input_SecondaryFire_Pressed);
			// EnhancedInputComponent->BindAction(SecondaryFireAction, ETriggerEvent::Completed, this, &AStrafeCharacter::Input_SecondaryFire_Released);
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
	UE_LOG(LogTemp, Warning, TEXT("AStrafeCharacter::Input_PrimaryFire_Pressed - Tag: %s, ASC: %s"),
		PrimaryFireInputTag.IsValid() ? *PrimaryFireInputTag.ToString() : TEXT("Invalid"),
		AbilitySystemComponent ? TEXT("Valid") : TEXT("Null"));

	if (AbilitySystemComponent && PrimaryFireInputTag.IsValid())
	{
		bool bSuccess = AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(PrimaryFireInputTag));
		UE_LOG(LogTemp, Warning, TEXT("AStrafeCharacter::Input_PrimaryFire_Pressed - TryActivateAbilitiesByTag returned: %s"),
			bSuccess ? TEXT("Success") : TEXT("Failed"));
	}
}

void AStrafeCharacter::Input_PrimaryFire_Released()
{
	if (AbilitySystemComponent)
	{
		// This would typically be used if your ability has a "Release" trigger
		// For now, we are activating on press. If you need to cancel an ability on release:
		// FGameplayTagContainer ReleaseTags;
		// ReleaseTags.AddTag(PrimaryFireInputTag); // Or a specific "Cancel" tag
		// AbilitySystemComponent->CancelAbilities(&ReleaseTags);
	}
}

void AStrafeCharacter::Input_SecondaryFire_Pressed()
{
	if (AbilitySystemComponent && SecondaryFireInputTag.IsValid()) // Check tag validity
	{
		AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(SecondaryFireInputTag)); // <<<<<<< CORRECTED
	}
}

void AStrafeCharacter::Input_SecondaryFire_Released()
{
	if (AbilitySystemComponent)
	{
		// Similar to PrimaryFire_Released
	}
}

void AStrafeCharacter::OnWeaponEquipped(ABaseWeapon* NewWeapon)
{
	UE_LOG(LogTemp, Warning, TEXT("AStrafeCharacter::OnWeaponEquipped called. NewWeapon: %s"), NewWeapon ? *NewWeapon->GetName() : TEXT("nullptr"));

	if (!AbilitySystemComponent || !HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("AStrafeCharacter::OnWeaponEquipped - Early exit. ASC: %s, HasAuthority: %s"),
			AbilitySystemComponent ? TEXT("Valid") : TEXT("Null"),
			HasAuthority() ? TEXT("True") : TEXT("False"));
		return;
	}

	// Clear any abilities granted by the previous weapon
	UE_LOG(LogTemp, Log, TEXT("AStrafeCharacter::OnWeaponEquipped - Clearing %d previous weapon abilities"), CurrentWeaponAbilityHandles.Num());
	for (FGameplayAbilitySpecHandle Handle : CurrentWeaponAbilityHandles)
	{
		AbilitySystemComponent->ClearAbility(Handle);
	}
	CurrentWeaponAbilityHandles.Empty();

	if (NewWeapon && NewWeapon->GetWeaponData())
	{
		UWeaponDataAsset* WeaponData = NewWeapon->GetWeaponData();
		UE_LOG(LogTemp, Log, TEXT("AStrafeCharacter::OnWeaponEquipped - Processing WeaponData: %s"), *WeaponData->GetName());

		// Grant Primary Fire Ability
		if (WeaponData->PrimaryFireAbility)
		{
			UE_LOG(LogTemp, Log, TEXT("AStrafeCharacter::OnWeaponEquipped - PrimaryFireAbility class: %s"),
				*WeaponData->PrimaryFireAbility->GetName());

			// First check if it's actually a UGA_WeaponActivate subclass
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
					FGameplayAbilitySpecHandle SpecHandle = AbilitySystemComponent->GiveAbility(
						FGameplayAbilitySpec(WeaponData->PrimaryFireAbility, 1, AbilityCDO->AbilityInputID, this)
					);
					CurrentWeaponAbilityHandles.Add(SpecHandle);
					UE_LOG(LogTemp, Log, TEXT("AStrafeCharacter::OnWeaponEquipped - Granted primary ability with InputID: %d"), AbilityCDO->AbilityInputID);
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
					FGameplayAbilitySpecHandle SpecHandle = AbilitySystemComponent->GiveAbility(
						FGameplayAbilitySpec(WeaponData->SecondaryFireAbility, 1, AbilityCDO->AbilityInputID, this)
					);
					CurrentWeaponAbilityHandles.Add(SpecHandle);
					UE_LOG(LogTemp, Log, TEXT("AStrafeCharacter::OnWeaponEquipped - Granted secondary ability with InputID: %d"), AbilityCDO->AbilityInputID);
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