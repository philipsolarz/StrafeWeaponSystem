// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ArenaPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerScoreChangedDelegate, int32, NewFrags, int32, NewDeaths);

/**
 * PlayerState for the Arena gamemode, tracking individual player scores.
 */
UCLASS(Blueprintable)
class STRAFEWEAPONSYSTEM_API AArenaPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AArenaPlayerState();

	//~ Begin APlayerState interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End APlayerState interface

	/**
	 * Called when a player scores a frag.
	 * @param KillerPlayerState The PlayerState of the killer.
	 * @param VictimPlayerState The PlayerState of the victim.
	 */
	void ScoreFrag(AArenaPlayerState* KillerPlayerState, AArenaPlayerState* VictimPlayerState);

	/**
	 * Called when a player dies.
	 */
	void ScoreDeath(AArenaPlayerState* KilledPlayerState, AArenaPlayerState* KillerPlayerState);

	UFUNCTION(BlueprintCallable, Category = "Arena|PlayerState")
	int32 GetFrags() const { return Frags; }

	UFUNCTION(BlueprintCallable, Category = "Arena|PlayerState")
	int32 GetDeaths() const { return Deaths; }

	/** Delegate called when this player's score (frags or deaths) changes. */
	UPROPERTY(BlueprintAssignable, Category = "Arena|PlayerState|Events")
	FOnPlayerScoreChangedDelegate OnPlayerScoreChanged;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Frags, BlueprintReadOnly, Category = "Arena|PlayerState")
	int32 Frags;

	UPROPERTY(ReplicatedUsing = OnRep_Deaths, BlueprintReadOnly, Category = "Arena|PlayerState")
	int32 Deaths;

	UFUNCTION()
	void OnRep_Frags();

	UFUNCTION()
	void OnRep_Deaths();

	/** Helper to broadcast score changes. */
	void BroadcastScoreUpdate();
};