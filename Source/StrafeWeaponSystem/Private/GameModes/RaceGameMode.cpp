#include "GameModes/RaceGameMode.h"
#include "Race/RaceManager.h"
#include "Player/RaceStateComponent.h" // To add to PlayerState
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameStateBase.h"


ARaceGameMode::ARaceGameMode()
{
	// Set default pawn class, player controller class, player state class etc.
	// PlayerStateClass = AYourPlayerStateWithRaceComponent::StaticClass(); // If you subclass PlayerState
	// For simplicity, we'll add the component directly in PostLogin if not already present.

	// Default RaceManager if not set in Blueprint
	RaceManagerClass = ARaceManager::StaticClass();
	CurrentRaceManager = nullptr;
	PlayerControllerClass = AStrafePlayerController::StaticClass();
}

void ARaceGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		if (RaceManagerClass)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			CurrentRaceManager = GetWorld()->SpawnActor<ARaceManager>(RaceManagerClass, SpawnParams);

			if (CurrentRaceManager)
			{
				//UE_LOG(LogTemp, Log, TEXT("RaceGameMode: Spawned RaceManager: %s"), *CurrentRaceManager->GetName());
				// Call refresh after a delay to ensure all checkpoints have had BeginPlay called.
				FTimerHandle TempHandle;
				GetWorldTimerManager().SetTimer(TempHandle, CurrentRaceManager, &ARaceManager::RefreshAllCheckpoints, 0.2f, false);
			}
			else
			{
				//UE_LOG(LogTemp, Error, TEXT("RaceGameMode: Failed to spawn RaceManager."));
			}
		}
		else
		{
			//UE_LOG(LogTemp, Error, TEXT("RaceGameMode: RaceManagerClass is not set!"));
		}
	}
}

void ARaceGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (NewPlayer)
	{
		APlayerState* PS = NewPlayer->PlayerState;
		if (PS && !PS->FindComponentByClass<URaceStateComponent>())
		{
			URaceStateComponent* RaceStateComp = NewObject<URaceStateComponent>(PS, TEXT("RaceStateComponent"));
			if (RaceStateComp)
			{
				RaceStateComp->RegisterComponent();
				//UE_LOG(LogTemp, Log, TEXT("RaceGameMode: Added RaceStateComponent to PlayerState: %s"), *PS->GetPlayerName());
				if (CurrentRaceManager)
				{
					CurrentRaceManager->UpdatePlayerInScoreboard(PS);
				}
			}
		}
		else if (PS && CurrentRaceManager)
		{
			// Player reconnected or already had component, ensure they are in scoreboard
			CurrentRaceManager->UpdatePlayerInScoreboard(PS);
		}
	}
}

void ARaceGameMode::InitGameState()
{
	Super::InitGameState();

	// A good place to let the GameState know about the RaceManager if other systems need access via GameState
	// For example, if you create a AYourGameState class:
	// AYourGameState* GS = GetGameState<AYourGameState>();
	// if (GS && CurrentRaceManager)
	// {
	//    GS->SetRaceManager(CurrentRaceManager);
	// }
}