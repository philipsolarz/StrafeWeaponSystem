// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/ArenaGamemode.h"
#include "GameModes/ArenaGameState.h"
#include "GameModes/ArenaPlayerState.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h" // For UGameplayStatics::GetPlayerState
#include "Player/StrafePlayerController.h" // Or your specific PlayerController class
#include "StrafeCharacter.h" // Or your specific Character class


AArenaGamemode::AArenaGamemode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.0f; // Tick once per second for timer updates primarily

	// Set default classes
	GameStateClass = AArenaGameState::StaticClass();
	PlayerStateClass = AArenaPlayerState::StaticClass();
	// PlayerControllerClass = AStrafePlayerController::StaticClass(); // Set in your project defaults or here
	// DefaultPawnClass = AStrafeCharacter::StaticClass(); // Set in your project defaults or here

	// Default rules
	MatchTimeLimitSeconds = 300; // 5 minutes
	FragLimit = 20;
}

void AArenaGamemode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	// Custom logic after a player logs in, e.g., update scoreboard displays if any
	// For Arena, player state will handle its own score initialization.
}

void AArenaGamemode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	// Custom logic when a player logs out
}

void AArenaGamemode::InitGameState()
{
	Super::InitGameState();

	AArenaGameState* ArenaGS = GetGameState<AArenaGameState>();
	if (ArenaGS)
	{
		ArenaGS->SetTimeRemaining(MatchTimeLimitSeconds);
		// Initial match state can be set here or in HandleMatchIsWaitingToStart
		 //ArenaGS->SetMatchState(MatchState); // MatchState is an FName, use GetMatchState() from AGameModeBase
	}
}

void AArenaGamemode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();

	AArenaGameState* ArenaGS = GetGameState<AArenaGameState>();
	if (ArenaGS)
	{
		ArenaGS->SetTimeRemaining(MatchTimeLimitSeconds); // Ensure timer is reset at match start
		ArenaGS->SetMatchState(MatchState); // Update GameState with current FName MatchState from AGameModeBase
	}

	StartMatchTimer();

	// Additional logic for when the match starts (e.g., enable player input, spawn initial pickups)
	// UE_LOG(LogTemp, Log, TEXT("Arena match has started. Time Limit: %d, Frag Limit: %d"), MatchTimeLimitSeconds, FragLimit);
}

void AArenaGamemode::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();

	AArenaGameState* ArenaGS = GetGameState<AArenaGameState>();
	if (ArenaGS)
	{
		ArenaGS->SetMatchState(MatchState); // Update GameState with current FName MatchState from AGameModeBase
	}

	GetWorldTimerManager().ClearTimer(MatchTimerHandle);
	// UE_LOG(LogTemp, Log, TEXT("Arena match has ended."));

	// Additional logic for when the match ends (e.g., disable input, show final scoreboard)
}


void AArenaGamemode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsMatchInProgress()) // Check if the match is currently in progress
	{
		// Match timer is handled by UpdateMatchTime via TimerManager for fixed updates.
		// CheckMatchEndConditions can be called here if continuous checking is preferred over event-driven.
		// However, for performance, it's better to check on events (kill, timer tick).
	}
}

void AArenaGamemode::OnPlayerFragged(AArenaPlayerState* FraggingPlayer)
{
	if (FraggingPlayer)
	{
		OnArenaPlayerScoreChanged_GM.Broadcast(FraggingPlayer);
	}
	CheckMatchEndConditions();
}

void AArenaGamemode::PlayerKilled(AController* VictimController, AController* KillerController, AActor* DamageCauser)
{
	AArenaPlayerState* VictimPlayerState = VictimController ? VictimController->GetPlayerState<AArenaPlayerState>() : nullptr;
	AArenaPlayerState* KillerPlayerState = KillerController ? KillerController->GetPlayerState<AArenaPlayerState>() : nullptr;

	if (VictimPlayerState)
	{
		VictimPlayerState->ScoreDeath(VictimPlayerState, KillerPlayerState);
	}

	if (KillerPlayerState)
	{
		// Killer gets a frag, unless it's a self-kill and victim is also killer
		if (VictimPlayerState != KillerPlayerState)
		{
			KillerPlayerState->ScoreFrag(KillerPlayerState, VictimPlayerState);
			// OnPlayerFragged will be called by the PlayerState, which then calls CheckMatchEndConditions
		}
		else
		{
			// Handle self-kill if specific scoring logic is needed (e.g. -1 frag)
			// For now, ScoreFrag in PlayerState already prevents self-frag scoring.
			// PlayerState's ScoreDeath will still record the death.
		}
	}
	// If no killer (e.g. environmental death), only victim's death is recorded.

	// Respawn logic would typically go here or be triggered from here
	// RestartPlayer(VictimController); // Example, customize as needed
}

void AArenaGamemode::CheckMatchEndConditions()
{
	if (!IsMatchInProgress()) // Don't check if match isn't running
	{
		return;
	}

	AArenaGameState* ArenaGS = GetGameState<AArenaGameState>();
	if (!ArenaGS) return;

	// Check Frag Limit
	if (FragLimit > 0)
	{
		for (APlayerState* PS : GameState->PlayerArray)
		{
			AArenaPlayerState* ArenaPS = Cast<AArenaPlayerState>(PS);
			if (ArenaPS && ArenaPS->GetFrags() >= FragLimit)
			{
				EndMatch(ArenaPS, FName(TEXT("FragLimitReached")));
				return; // Match ended
			}
		}
	}

	// Time Limit is checked by UpdateMatchTime, which calls EndMatch if time runs out.
	// If MatchTimeLimitSeconds is 0, UpdateMatchTime won't end the match due to time.
}

void AArenaGamemode::EndMatch(APlayerState* Winner, FName Reason)
{
	Super::EndMatch(); // This will set the match state to WaitingPostMatch or similar

	// Broadcast the custom delegate
	OnArenaMatchEnded.Broadcast(Winner, Reason);

	// Log who won and why
	// if (Winner)
	// {
	// 	UE_LOG(LogTemp, Log, TEXT("Match Ended. Winner: %s. Reason: %s"), *Winner->GetPlayerName(), *Reason.ToString());
	// }
	// else
	// {
	// 	UE_LOG(LogTemp, Log, TEXT("Match Ended. Draw or No Winner. Reason: %s"), *Reason.ToString());
	// }
}

void AArenaGamemode::StartMatchTimer()
{
	if (MatchTimeLimitSeconds > 0)
	{
		GetWorldTimerManager().SetTimer(MatchTimerHandle, this, &AArenaGamemode::UpdateMatchTime, 1.0f, true);
		// UE_LOG(LogTemp, Log, TEXT("Match timer started for %d seconds."), MatchTimeLimitSeconds);
	}
}

void AArenaGamemode::UpdateMatchTime()
{
	AArenaGameState* ArenaGS = GetGameState<AArenaGameState>();
	if (ArenaGS && IsMatchInProgress())
	{
		int32 CurrentTime = ArenaGS->GetTimeRemaining();
		CurrentTime--;
		ArenaGS->SetTimeRemaining(CurrentTime);

		if (CurrentTime <= 0)
		{
			// Time limit reached. Determine winner based on frags, or draw.
			APlayerState* Winner = nullptr;
			int32 MaxFrags = -1;
			bool bIsDraw = false;

			if (GameState && GameState->PlayerArray.Num() > 0)
			{
				for (APlayerState* PS : GameState->PlayerArray)
				{
					AArenaPlayerState* ArenaPS = Cast<AArenaPlayerState>(PS);
					if (ArenaPS)
					{
						if (ArenaPS->GetFrags() > MaxFrags)
						{
							MaxFrags = ArenaPS->GetFrags();
							Winner = ArenaPS;
							bIsDraw = false;
						}
						else if (ArenaPS->GetFrags() == MaxFrags && MaxFrags != -1) // MaxFrags != -1 ensures we don't draw if everyone has 0
						{
							bIsDraw = true;
						}
					}
				}
			}


			if (bIsDraw) Winner = nullptr; // Explicitly nullify winner in case of a draw
			EndMatch(Winner, FName(TEXT("TimeLimitReached")));
		}
	}
	else if (!IsMatchInProgress())
	{
		// Clear timer if match is no longer in progress for some other reason
		GetWorldTimerManager().ClearTimer(MatchTimerHandle);
	}
}


int32 AArenaGamemode::GetTimeRemaining() const
{
	const AArenaGameState* ArenaGS = GetGameState<AArenaGameState>();
	return ArenaGS ? ArenaGS->GetTimeRemaining() : 0;
}

int32 AArenaGamemode::GetPlayerFrags(APlayerController* PlayerController) const
{
	if (PlayerController)
	{
		const AArenaPlayerState* ArenaPS = PlayerController->GetPlayerState<AArenaPlayerState>();
		return ArenaPS ? ArenaPS->GetFrags() : 0;
	}
	return 0;
}

int32 AArenaGamemode::GetPlayerDeaths(APlayerController* PlayerController) const
{
	if (PlayerController)
	{
		const AArenaPlayerState* ArenaPS = PlayerController->GetPlayerState<AArenaPlayerState>();
		return ArenaPS ? ArenaPS->GetDeaths() : 0;
	}
	return 0;
}