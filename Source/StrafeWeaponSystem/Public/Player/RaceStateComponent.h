#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RaceStateComponent.generated.h"

USTRUCT(BlueprintType)
struct FPlayerRaceTime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	float TotalTime;

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	TArray<float> SplitTimes; // Time at each checkpoint relative to start

	FPlayerRaceTime() : TotalTime(0.0f) {}
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerRaceStateChanged, class URaceStateComponent*, RaceState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerNewBestTime, const FPlayerRaceTime&, BestTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerCheckpointHit, int32, CheckpointIndex, float, TimeAtCheckpoint);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerFinishedRace, float, FinalTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerRaceStarted);


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class STRAFEWEAPONSYSTEM_API URaceStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URaceStateComponent();

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentRaceTime, BlueprintReadOnly, Category = "Race")
	float CurrentRaceTime;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentSplitTimes, BlueprintReadOnly, Category = "Race")
	TArray<float> CurrentSplitTimes;

	UPROPERTY(ReplicatedUsing = OnRep_BestRaceTime, BlueprintReadOnly, Category = "Race")
	FPlayerRaceTime BestRaceTime;

	UPROPERTY(ReplicatedUsing = OnRep_LastCheckpointReached, BlueprintReadOnly, Category = "Race")
	int32 LastCheckpointReached; // -1 if no checkpoint hit yet in current run

	UPROPERTY(ReplicatedUsing = OnRep_IsRaceActiveForPlayer, BlueprintReadOnly, Category = "Race")
	bool bIsRaceActiveForPlayer; // Is the timer currently running for this player?

	FTimerHandle RaceTimerHandle;

public:
	// Called by RaceManager or StartTrigger
	UFUNCTION(BlueprintCallable, Category = "Race")
	void StartRace();

	// Called by CheckpointTrigger (via RaceManager)
	UFUNCTION(BlueprintCallable, Category = "Race")
	void ReachedCheckpoint(int32 CheckpointIndex, int32 TotalCheckpointsInRace);

	// Called by FinishTrigger (via RaceManager)
	UFUNCTION(BlueprintCallable, Category = "Race")
	void FinishedRace(int32 FinalCheckpointIndex, int32 TotalCheckpointsInRace);

	UFUNCTION(BlueprintCallable, Category = "Race")
	void ResetRaceState();

	UFUNCTION(BlueprintPure, Category = "Race")
	float GetCurrentRaceTime() const { return CurrentRaceTime; }

	UFUNCTION(BlueprintPure, Category = "Race")
	const TArray<float>& GetCurrentSplitTimes() const { return CurrentSplitTimes; }

	UFUNCTION(BlueprintPure, Category = "Race")
	const FPlayerRaceTime& GetBestRaceTime() const { return BestRaceTime; }

	UFUNCTION(BlueprintPure, Category = "Race")
	int32 GetLastCheckpointReached() const { return LastCheckpointReached; }

	UFUNCTION(BlueprintPure, Category = "Race")
	bool IsRaceInProgress() const { return bIsRaceActiveForPlayer; }

	UPROPERTY(BlueprintAssignable, Category = "Race Events")
	FOnPlayerRaceStateChanged OnPlayerRaceStateChanged; // Generic state change

	UPROPERTY(BlueprintAssignable, Category = "Race Events")
	FOnPlayerNewBestTime OnPlayerNewBestTime;

	UPROPERTY(BlueprintAssignable, Category = "Race Events")
	FOnPlayerCheckpointHit OnPlayerCheckpointHit;

	UPROPERTY(BlueprintAssignable, Category = "Race Events")
	FOnPlayerFinishedRace OnPlayerFinishedRace;

	UPROPERTY(BlueprintAssignable, Category = "Race Events")
	FOnPlayerRaceStarted OnPlayerRaceStarted;


private:
	UFUNCTION()
	void UpdateTimer();

	UFUNCTION()
	void OnRep_CurrentRaceTime();

	UFUNCTION()
	void OnRep_CurrentSplitTimes();

	UFUNCTION()
	void OnRep_BestRaceTime();

	UFUNCTION()
	void OnRep_LastCheckpointReached();

	UFUNCTION()
	void OnRep_IsRaceActiveForPlayer();

	void NotifyStateChange();
};