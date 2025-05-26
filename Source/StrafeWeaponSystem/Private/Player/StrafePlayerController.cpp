#include "Player/StrafePlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Blueprint/UserWidget.h"
#include "InputAction.h"
#include "InputMappingContext.h" // Include this

AStrafePlayerController::AStrafePlayerController()
{
	bIsScoreboardVisible = false;
	ScoreboardWidgetInstance = nullptr;
}

void AStrafePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (ScoreboardMappingContext)
		{
			Subsystem->AddMappingContext(ScoreboardMappingContext, 1); // UI context, higher priority
		}
		else
		{
			//UE_LOG(LogTemp, Warning, TEXT("StrafePlayerController: ScoreboardMappingContext is not set!"));
		}
	}
}

void AStrafePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EnhancedPlayerInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (ToggleScoreboardAction)
		{
			EnhancedPlayerInputComponent->BindAction(ToggleScoreboardAction, ETriggerEvent::Started, this, &AStrafePlayerController::ToggleScoreboard);
		}
		else
		{
			//UE_LOG(LogTemp, Warning, TEXT("StrafePlayerController: ToggleScoreboardAction is not set!"));
		}
	}
}

void AStrafePlayerController::ToggleScoreboard()
{
	if (!ScoreboardWidgetClass)
	{
		//UE_LOG(LogTemp, Warning, TEXT("StrafePlayerController: ScoreboardWidgetClass is not set. Cannot toggle scoreboard."));
		return;
	}

	if (bIsScoreboardVisible)
	{
		if (ScoreboardWidgetInstance)
		{
			ScoreboardWidgetInstance->RemoveFromParent();
			// ScoreboardWidgetInstance = nullptr; // Let it be GC'd, or manage pooling if preferred
		}
		bIsScoreboardVisible = false;
		FInputModeGameOnly InputModeData;
		SetInputMode(InputModeData);
		SetShowMouseCursor(false);
	}
	else
	{
		if (!ScoreboardWidgetInstance) // Create if it doesn't exist (e.g., first time or was GC'd)
		{
			ScoreboardWidgetInstance = CreateWidget<UUserWidget>(this, ScoreboardWidgetClass);
		}

		if (ScoreboardWidgetInstance)
		{
			ScoreboardWidgetInstance->AddToViewport();
			bIsScoreboardVisible = true;
			FInputModeGameAndUI InputModeData;
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			InputModeData.SetHideCursorDuringCapture(false);
			SetInputMode(InputModeData);
			SetShowMouseCursor(true);
		}
		else
		{
			//UE_LOG(LogTemp, Error, TEXT("StrafePlayerController: Failed to create ScoreboardWidgetInstance."));
		}
	}
}

void AStrafePlayerController::SetInputMode(FInputModeDataBase const& InData)
{
	Super::SetInputMode(InData); // Call the base class implementation

	// If you need specific logic for your game (e.g. unpausing, etc.)
	// based on input mode changes, you can add it here.
	// For scoreboard, the main thing is calling Super and setting bShowMouseCursor appropriately.
}