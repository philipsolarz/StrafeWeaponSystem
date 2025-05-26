#include "Player/RaceStateComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h" // For GEngine

URaceStateComponent::URaceStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true); // Ensure this component is replicated

	CurrentRaceTime = 0.0f;
	LastCheckpointReached = -1;
	bIsRaceActiveForPlayer = false;
	BestRaceTime.TotalTime = -1.0f;
}

void URaceStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(URaceStateComponent, CurrentRaceTime);
	DOREPLIFETIME(URaceStateComponent, CurrentSplitTimes);
	DOREPLIFETIME(URaceStateComponent, BestRaceTime);
	DOREPLIFETIME(URaceStateComponent, LastCheckpointReached);
	DOREPLIFETIME(URaceStateComponent, bIsRaceActiveForPlayer);
}

void URaceStateComponent::BeginPlay()
{
	Super::BeginPlay();
	// Ensure owner is a PlayerState
	if (!GetOwner() || !GetOwner()->IsA<APlayerState>())
	{
		//UE_LOG(LogTemp, Error, TEXT("RaceStateComponent is not attached to a PlayerState!"));
		// Consider destroying or deactivating component if not on a PlayerState
	}
}

void URaceStateComponent::StartRace()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		//UE_LOG(LogTemp, Warning, TEXT("Player %s started race."), *GetOwner()->GetName());
		CurrentRaceTime = 0.0f;
		CurrentSplitTimes.Empty();
		LastCheckpointReached = -1; // Start line is usually index 0, so -1 means not even start is hit.
		bIsRaceActiveForPlayer = true;

		GetWorld()->GetTimerManager().SetTimer(RaceTimerHandle, this, &URaceStateComponent::UpdateTimer, 0.01f, true);

		// Call OnRep for server itself to update its state for local logic if needed
		OnRep_IsRaceActiveForPlayer();
		OnRep_CurrentRaceTime();
		OnRep_CurrentSplitTimes();
		OnRep_LastCheckpointReached();
		NotifyStateChange();
		OnPlayerRaceStarted.Broadcast();
	}
}

void URaceStateComponent::ReachedCheckpoint(int32 CheckpointIndex, int32 TotalCheckpointsInRace)
{
	if (GetOwner() && GetOwner()->HasAuthority() && bIsRaceActiveForPlayer)
	{
		// Ensure checkpoints are hit in order (or allow out-of-order if design permits)
		// For simplicity, we assume they must be in order, meaning CheckpointIndex must be LastCheckpointReached + 1
		// The Start line itself is a checkpoint (index 0).
		if (CheckpointIndex == LastCheckpointReached + 1)
		{
			LastCheckpointReached = CheckpointIndex;
			CurrentSplitTimes.Add(CurrentRaceTime);
			//UE_LOG(LogTemp, Warning, TEXT("Player %s reached checkpoint %d at time %f. Total Splits: %d"), *GetOwner()->GetName(), CheckpointIndex, CurrentRaceTime, CurrentSplitTimes.Num());

			OnRep_LastCheckpointReached(); // For server
			OnRep_CurrentSplitTimes(); // For server
			NotifyStateChange();
			OnPlayerCheckpointHit.Broadcast(CheckpointIndex, CurrentRaceTime);
		}
		else
		{
			//UE_LOG(LogTemp, Warning, TEXT("Player %s attempted to hit checkpoint %d out of order. Last hit: %d"), *GetOwner()->GetName(), CheckpointIndex, LastCheckpointReached);
		}
	}
}

void URaceStateComponent::FinishedRace(int32 FinalCheckpointIndex, int32 TotalCheckpointsInRace)
{
	if (GetOwner() && GetOwner()->HasAuthority() && bIsRaceActiveForPlayer)
	{
		// --- Add UE_LOGs here for debugging the values in this function ---
		UE_LOG(LogTemp, Warning, TEXT("URaceStateComponent::FinishedRace for %s: LastCheckpointReached: %d, FinalCheckpointIndex (param): %d, CurrentSplitTimes.Num(): %d, TotalCheckpointsInRace (param): %d"),
			*GetOwner()->GetName(),
			LastCheckpointReached,
			FinalCheckpointIndex,
			CurrentSplitTimes.Num(),
			TotalCheckpointsInRace);
		// --- End UE_LOGs ---

		// Corrected Condition:
		// 1. LastCheckpointReached should now be the index of the finish line because ReachedCheckpoint was called for it.
		// 2. CurrentSplitTimes.Num() should be equal to TotalCheckpointsInRace, as a split is added for every checkpoint from Start to Finish inclusive.
		if (LastCheckpointReached == FinalCheckpointIndex && CurrentSplitTimes.Num() == TotalCheckpointsInRace)
		{
			UE_LOG(LogTemp, Warning, TEXT("URaceStateComponent::FinishedRace - CONDITIONS MET for %s! Processing finish."), *GetOwner()->GetName());
			bIsRaceActiveForPlayer = false;
			GetWorld()->GetTimerManager().ClearTimer(RaceTimerHandle);

			// UE_LOG(LogTemp, Warning, TEXT("Player %s finished race at time %f."), *GetOwner()->GetName(), CurrentRaceTime); // Already logged above effectively

			if (BestRaceTime.TotalTime < 0.0f || CurrentRaceTime < BestRaceTime.TotalTime)
			{
				UE_LOG(LogTemp, Warning, TEXT("Player %s got a NEW BEST TIME! New: %f, Old was: %f"), *GetOwner()->GetName(), CurrentRaceTime, BestRaceTime.TotalTime);
				BestRaceTime.TotalTime = CurrentRaceTime;
				BestRaceTime.SplitTimes = CurrentSplitTimes;
				if (GetOwner() && GetOwner()->HasAuthority()) // Ensure server directly triggers its own OnRep logic if effects are desired immediately
				{
					OnRep_BestRaceTime();
				}
				OnPlayerNewBestTime.Broadcast(BestRaceTime);
			}

			if (GetOwner() && GetOwner()->HasAuthority()) // Ensure server directly triggers its own OnRep logic
			{
				OnRep_IsRaceActiveForPlayer();
			}
			NotifyStateChange();
			OnPlayerFinishedRace.Broadcast(CurrentRaceTime); // This should now fire!
			UE_LOG(LogTemp, Warning, TEXT("URaceStateComponent::FinishedRace - OnPlayerFinishedRace BROADCAST for %s with time %f."), *GetOwner()->GetName(), CurrentRaceTime);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("URaceStateComponent::FinishedRace - CONDITIONS NOT MET for %s. Race not finished properly. LastCP: %d (Expected %d), Splits.Num(): %d (Expected %d)"),
				*GetOwner()->GetName(), LastCheckpointReached, FinalCheckpointIndex, CurrentSplitTimes.Num(), TotalCheckpointsInRace);

			// Consider what to do if conditions are not met. For now, it just logs.
			// ResetRaceState(); // You might want to uncomment this if a failed finish means the run is void.
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("URaceStateComponent::FinishedRace for %s: Not processed because GetOwner()->HasAuthority() is %s OR bIsRaceActiveForPlayer is %s."),
			GetOwner() ? *GetOwner()->GetName() : TEXT("UnknownOwner"),
			(GetOwner() && GetOwner()->HasAuthority()) ? TEXT("true") : TEXT("false"),
			bIsRaceActiveForPlayer ? TEXT("true") : TEXT("false"));
	}
}

void URaceStateComponent::ResetRaceState()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		//UE_LOG(LogTemp, Warning, TEXT("Player %s race state reset."), *GetOwner()->GetName());
		CurrentRaceTime = 0.0f;
		CurrentSplitTimes.Empty();
		LastCheckpointReached = -1;
		bIsRaceActiveForPlayer = false;
		GetWorld()->GetTimerManager().ClearTimer(RaceTimerHandle);

		OnRep_IsRaceActiveForPlayer();
		OnRep_CurrentRaceTime();
		OnRep_CurrentSplitTimes();
		OnRep_LastCheckpointReached();
		NotifyStateChange();
	}
}

void URaceStateComponent::UpdateTimer()
{
	if (GetOwner() && GetOwner()->HasAuthority() && bIsRaceActiveForPlayer)
	{
		CurrentRaceTime += GetWorld()->GetTimerManager().GetTimerElapsed(RaceTimerHandle);
		// No need to call OnRep_CurrentRaceTime here every tick, it's replicated.
		// Clients will get updated CurrentRaceTime. The HUD will poll this.
		// If you want more frequent updates for server-side logic based on time, this is fine.
		// Consider if a less frequent update or event-based update for server logic is better.
		NotifyStateChange(); // Or a more specific "TimerUpdated" event if needed
	}
}

void URaceStateComponent::OnRep_CurrentRaceTime()
{
	NotifyStateChange();
}

void URaceStateComponent::OnRep_CurrentSplitTimes()
{
	NotifyStateChange();
	// When splits replicate, if the last split corresponds to a known checkpoint index, fire the event
	// This is tricky because OnRep_LastCheckpointReached might not have fired yet.
	// It's safer to rely on the server to fire the OnPlayerCheckpointHit event,
	// and clients use the replicated data for HUD.
}

void URaceStateComponent::OnRep_BestRaceTime()
{
	OnPlayerNewBestTime.Broadcast(BestRaceTime);
	NotifyStateChange();
}

void URaceStateComponent::OnRep_LastCheckpointReached()
{
	// This is mostly for client-side prediction or HUD updates.
	// The actual OnPlayerCheckpointHit should be fired by the server when logic dictates.
	// If CurrentSplitTimes has an entry for this index, fire client-side event.
	//if (CurrentSplitTimes.IsValidIndex(LastCheckpointReached) && LastCheckpointReached >= 0) // LastCheckpointReached can be -1
	//{
	//	OnPlayerCheckpointHit.Broadcast(LastCheckpointReached, CurrentSplitTimes[LastCheckpointReached]);
	//}
	NotifyStateChange();
}

void URaceStateComponent::OnRep_IsRaceActiveForPlayer()
{
	if (bIsRaceActiveForPlayer) {
		OnPlayerRaceStarted.Broadcast();
	}
	else {
		// If race becomes inactive and CurrentRaceTime > 0, it implies a finish or reset.
		// The specific OnPlayerFinishedRace is handled by the server logic.
		// This OnRep is more about the timer visibility/state on HUD.
	}
	NotifyStateChange();
}

void URaceStateComponent::NotifyStateChange()
{
	// This is a generic notification. Useful for UMG to update.
	OnPlayerRaceStateChanged.Broadcast(this);
	if (GEngine && GetOwner() && GetOwner()->GetNetMode() != NM_DedicatedServer)
	{
		APlayerState* PS = Cast<APlayerState>(GetOwner());
		// This log can be very spammy with UpdateTimer.
		// GEngine->AddOnScreenDebugMessage(-1, 0.01f, FColor::Cyan, FString::Printf(TEXT("%s - Active: %s, Time: %.2f, LastCP: %d, Splits: %d"),
		// 	PS ? *PS->GetPlayerName() : TEXT("UnknownPlayer"),
		// 	bIsRaceActiveForPlayer ? TEXT("Yes") : TEXT("No"),
		// 	CurrentRaceTime,
		// 	LastCheckpointReached,
		// 	CurrentSplitTimes.Num()
		// ));
	}
}