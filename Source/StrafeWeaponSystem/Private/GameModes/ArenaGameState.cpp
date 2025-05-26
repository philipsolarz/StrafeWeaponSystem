// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/ArenaGameState.h"
#include "Net/UnrealNetwork.h"

AArenaGameState::AArenaGameState()
{
	TimeRemaining = 0;
	// MatchState will be set by GameMode, typically starting with WaitingToStart or InProgress.
	// Consider what the initial default should be if not immediately overridden.
	MatchState = FName(TEXT("WaitingToStart"));
}

void AArenaGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AArenaGameState, TimeRemaining);
	DOREPLIFETIME(AArenaGameState, MatchState);
}

void AArenaGameState::SetTimeRemaining(int32 NewTime)
{
	if (HasAuthority())
	{
		TimeRemaining = NewTime;
		OnRep_TimeRemaining(); // Call on server too for local effects
		OnTimeRemainingChanged.Broadcast(TimeRemaining); // Also broadcast directly on server
	}
}

void AArenaGameState::SetMatchState(FName NewState)
{
	if (HasAuthority())
	{
		MatchState = NewState;
		OnRep_MatchState(); // Call on server for local effects
		OnArenaMatchStateChanged.Broadcast(MatchState); // Also broadcast directly on server
	}
}

void AArenaGameState::OnRep_TimeRemaining()
{
	OnTimeRemainingChanged.Broadcast(TimeRemaining);
}

void AArenaGameState::OnRep_MatchState()
{
	OnArenaMatchStateChanged.Broadcast(MatchState);
}