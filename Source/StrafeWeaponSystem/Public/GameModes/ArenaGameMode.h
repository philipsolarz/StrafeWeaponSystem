// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameModes/StrafeGameMode.h" // Your base gamemode
#include "ArenaGamemode.generated.h"

class AArenaGameState;
class AArenaPlayerState;

// Delegate for when the match ends. Carries winning player (if any) and reason.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnArenaMatchEndedDelegate, APlayerState*, WinningPlayer, FName, Reason);
// Delegate for when a player's score changes, primarily for GameMode level notifications if needed,
// individual score changes are on ArenaPlayerState.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnArenaPlayerScoreChangedGMDelegate, AArenaPlayerState*, ScoredPlayerState);


/**
 * ArenaGamemode: Free-for-all with configurable time and frag limits.
 */
UCLASS(Blueprintable)
class STRAFEWEAPONSYSTEM_API AArenaGamemode : public AStrafeGameMode
{
	GENERATED_BODY()

public:
	AArenaGamemode();

	//~ Begin AGameModeBase Interface
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void InitGameState() override;
	virtual void HandleMatchHasStarted() override;
	virtual void HandleMatchHasEnded() override;
	virtual void Tick(float DeltaSeconds) override;
	//~ End AGameModeBase Interface

	/** Called by ArenaPlayerState when a player scores a frag. GameMode uses this to check for FragLimit. */
	virtual void OnPlayerFragged(AArenaPlayerState* FraggingPlayer);

	/**
	 * Called when a character is killed.
	 * @param VictimController Controller of the character that was killed.
	 * @param KillerController Controller of the character that did the killing.
	 * @param DamageCauser Actor that caused the damage.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arena|Gamemode")
	virtual void PlayerKilled(AController* VictimController, AController* KillerController, AActor* DamageCauser);

	/** Match time limit in seconds. 0 means no time limit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Rules")
	int32 MatchTimeLimitSeconds;

	/** Frag limit for the match. 0 means no frag limit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Rules")
	int32 FragLimit;

	/** Delegate broadcast when the match ends. */
	UPROPERTY(BlueprintAssignable, Category = "Arena|Gamemode|Events")
	FOnArenaMatchEndedDelegate OnArenaMatchEnded;

	/** Delegate broadcast when any player's score changes. (Primarily for server-side game logic reacting to score events) */
	UPROPERTY(BlueprintAssignable, Category = "Arena|Gamemode|Events")
	FOnArenaPlayerScoreChangedGMDelegate OnArenaPlayerScoreChanged_GM;

	// --- Custom EndMatch for Arena with parameters ---
	/**
	 * Ends the match with a specific winner and reason.
	 * This is the primary way this Arena gamemode expects the match to be concluded with specific outcomes.
	 * It internally calls the base AGameMode::EndMatch().
	 */
	virtual void EndMatch(APlayerState* Winner, FName Reason);

	// --- Unhide AGameMode's parameterless EndMatch ---
	/** * By defining our own EndMatch with a different signature, the base class's EndMatch() is hidden.
	 * This 'using' declaration makes AGameMode::EndMatch() visible again in AArenaGamemode's scope.
	 * This helps resolve warning C4264 and clarifies that both versions are accessible if needed,
	 * though our primary path for ending the Arena match with specific outcomes is via the two-parameter version.
	 */
	using AGameMode::EndMatch;


	// --- Blueprint Callable Getters ---

	UFUNCTION(BlueprintCallable, Category = "Arena|Rules")
	int32 GetCurrentMatchTimeLimit() const { return MatchTimeLimitSeconds; }

	UFUNCTION(BlueprintCallable, Category = "Arena|Rules")
	int32 GetCurrentFragLimit() const { return FragLimit; }

	UFUNCTION(BlueprintCallable, Category = "Arena|GameState")
	int32 GetTimeRemaining() const;

	UFUNCTION(BlueprintCallable, Category = "Arena|Player")
	int32 GetPlayerFrags(APlayerController* PlayerController) const;

	UFUNCTION(BlueprintCallable, Category = "Arena|Player")
	int32 GetPlayerDeaths(APlayerController* PlayerController) const;

protected:
	FTimerHandle MatchTimerHandle;

	/** Checks if the match should end due to time limit or frag limit. */
	virtual void CheckMatchEndConditions();

	// EndMatch(APlayerState* Winner, FName Reason); // This was the old location of the declaration, moved to public

	/** Starts the match timer if MatchTimeLimitSeconds > 0 */
	void StartMatchTimer();

	/** Called by MatchTimerHandle to decrement time and check end conditions. */
	void UpdateMatchTime();

	/**
	 * How to extend in Blueprints:
	 * - Override `HandleMatchHasStarted` to implement custom logic when the match begins (e.g., spawn items, play sounds).
	 * - Override `HandleMatchHasEnded` to implement custom logic when the match ends (e.g., display scoreboard, change level).
	 * - Override `PlayerKilled` to add custom logic upon a player's death (e.g., special effects, announcer voice).
	 * - Bind to `OnArenaMatchEnded` in Blueprints to react to the match ending event.
	 * - Game settings like `MatchTimeLimitSeconds` and `FragLimit` can be set in Blueprint subclasses of this GameMode.
	 */
};