#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/RaceStateComponent.h" // For FPlayerRaceTime
#include "RaceManager.generated.h"

class ACheckpointTrigger;
class AStrafeCharacter;
class APlayerState;


USTRUCT(BlueprintType)
struct FPlayerScoreboardEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	FString PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	FPlayerRaceTime BestTime;

	// Store the PlayerState to potentially retrieve more info if needed
	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerState> PlayerStateRef;


	FPlayerScoreboardEntry()
	{
		PlayerName = TEXT("N/A");
		BestTime.TotalTime = FLT_MAX;
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnScoreboardUpdated);

UCLASS(Blueprintable, BlueprintType)
class STRAFEWEAPONSYSTEM_API ARaceManager : public AActor
{
	GENERATED_BODY()

public:
	ARaceManager();

protected:
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Race")
	TArray<ACheckpointTrigger*> AllCheckpointsInOrder; // Automatically populated and sorted

	UPROPERTY(ReplicatedUsing = OnRep_Scoreboard, BlueprintReadOnly, Category = "Race")
	TArray<FPlayerScoreboardEntry> Scoreboard;

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	ACheckpointTrigger* StartLine;

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	ACheckpointTrigger* FinishLine;

	int32 TotalCheckpointsForFullLap; // Includes start and finish

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	UFUNCTION(BlueprintCallable, Category = "Race")
	void RegisterCheckpoint(ACheckpointTrigger* Checkpoint);

	UFUNCTION()
	void HandleCheckpointReached(ACheckpointTrigger* Checkpoint, AStrafeCharacter* PlayerCharacter);

	// Called when a player joins or an existing player's data might need updating
	UFUNCTION(BlueprintCallable, Category = "Race")
	void UpdatePlayerInScoreboard(APlayerState* PlayerState);

	UFUNCTION(BlueprintCallable, Category = "Race")
	void RefreshAllCheckpoints();


	UFUNCTION(BlueprintPure, Category = "Race")
	const TArray<FPlayerScoreboardEntry>& GetScoreboard() const { return Scoreboard; }

	UPROPERTY(BlueprintAssignable, Category = "Race Events")
	FOnScoreboardUpdated OnScoreboardUpdated;


private:
	UFUNCTION()
	void OnRep_Scoreboard();

	void SortCheckpoints();
	void InitializeRaceSetup();
};