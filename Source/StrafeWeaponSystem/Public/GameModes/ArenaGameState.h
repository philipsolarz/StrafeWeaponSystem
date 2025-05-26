// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ArenaGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTimeRemainingChangedDelegate, int32, NewTimeRemaining);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnArenaMatchStateChangedDelegate, FName, NewState); // Using FName for match state for flexibility

/**
 * GameState for the Arena gamemode, tracking overall match state.
 */
UCLASS(Blueprintable)
class STRAFEWEAPONSYSTEM_API AArenaGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AArenaGameState();

	//~ Begin AGameStateBase interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End AGameStateBase interface

	UFUNCTION(BlueprintCallable, Category = "Arena|GameState")
	int32 GetTimeRemaining() const { return TimeRemaining; }

	void SetTimeRemaining(int32 NewTime);

	/** Delegate called when the replicated time remaining changes. */
	UPROPERTY(BlueprintAssignable, Category = "Arena|GameState|Events")
	FOnTimeRemainingChangedDelegate OnTimeRemainingChanged;

	/** Delegate called when the overall match state changes (e.g., InProgress, WaitingPostMatch). */
	UPROPERTY(BlueprintAssignable, Category = "Arena|GameState|Events")
	FOnArenaMatchStateChangedDelegate OnArenaMatchStateChanged;


	UFUNCTION(BlueprintCallable, Category = "Arena|GameState")
	FName GetMatchState() const { return MatchState; }

	void SetMatchState(FName NewState);


protected:
	UPROPERTY(ReplicatedUsing = OnRep_TimeRemaining, BlueprintReadOnly, Category = "Arena|GameState")
	int32 TimeRemaining; // In seconds

	// Match state (e.g., WaitingToStart, InProgress, WaitingPostMatch, etc.)
	// Using FName for flexibility, but you could use an enum.
	UPROPERTY(ReplicatedUsing = OnRep_MatchState)
	FName MatchState;


	UFUNCTION()
	void OnRep_TimeRemaining();

	UFUNCTION()
	void OnRep_MatchState();
};