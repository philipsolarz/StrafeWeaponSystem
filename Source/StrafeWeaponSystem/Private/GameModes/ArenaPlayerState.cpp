// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/ArenaPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "GameModes/ArenaGamemode.h" // To potentially notify GameMode

AArenaPlayerState::AArenaPlayerState()
{
	Frags = 0;
	Deaths = 0;
}

void AArenaPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AArenaPlayerState, Frags);
	DOREPLIFETIME(AArenaPlayerState, Deaths);
}

void AArenaPlayerState::ScoreFrag(AArenaPlayerState* KillerPlayerState, AArenaPlayerState* VictimPlayerState)
{
	// This function is typically called on the server by the GameMode.
	// If this PlayerState belongs to the killer.
	if (this == KillerPlayerState && KillerPlayerState != VictimPlayerState) // Prevent self-kill counting as a frag
	{
		Frags++;
		BroadcastScoreUpdate(); // For server-side systems

		// Potentially notify GameMode about the score change if it needs to check for FragLimit
		if (GetWorld() && GetWorld()->GetAuthGameMode<AArenaGamemode>())
		{
			GetWorld()->GetAuthGameMode<AArenaGamemode>()->OnPlayerFragged(this);
		}
	}
}

void AArenaPlayerState::ScoreDeath(AArenaPlayerState* KilledPlayerState, AArenaPlayerState* KillerPlayerState)
{
	// This function is typically called on the server by the GameMode.
	// If this PlayerState belongs to the one who died.
	if (this == KilledPlayerState)
	{
		Deaths++;
		BroadcastScoreUpdate(); // For server-side systems
	}
}

void AArenaPlayerState::OnRep_Frags()
{
	BroadcastScoreUpdate();
}

void AArenaPlayerState::OnRep_Deaths()
{
	BroadcastScoreUpdate();
}

void AArenaPlayerState::BroadcastScoreUpdate()
{
	OnPlayerScoreChanged.Broadcast(Frags, Deaths);
}