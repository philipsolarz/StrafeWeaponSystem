#include "Race/RaceManager.h"
#include "Race/CheckpointTrigger.h"
#include "Player/RaceStateComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "StrafeCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"

ARaceManager::ARaceManager()
{
	PrimaryActorTick.bCanEverTick = false; // Can be true if needed for updates
	bReplicates = true;
	StartLine = nullptr;
	FinishLine = nullptr;
	TotalCheckpointsForFullLap = 0;
}

void ARaceManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARaceManager, AllCheckpointsInOrder);
	DOREPLIFETIME(ARaceManager, Scoreboard);
}

void ARaceManager::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		RefreshAllCheckpoints(); // Initial population
	}
}

void ARaceManager::RefreshAllCheckpoints()
{
	if (!HasAuthority()) return;

	AllCheckpointsInOrder.Empty();
	StartLine = nullptr;
	FinishLine = nullptr;

	TArray<AActor*> FoundCheckpointActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACheckpointTrigger::StaticClass(), FoundCheckpointActors);

	for (AActor* Actor : FoundCheckpointActors)
	{
		ACheckpointTrigger* CP = Cast<ACheckpointTrigger>(Actor);
		if (CP)
		{
			RegisterCheckpoint(CP);
		}
	}
	InitializeRaceSetup(); // This will sort and assign Start/Finish
}


void ARaceManager::RegisterCheckpoint(ACheckpointTrigger* Checkpoint)
{
	if (Checkpoint && HasAuthority())
	{
		if (!AllCheckpointsInOrder.Contains(Checkpoint))
		{
			AllCheckpointsInOrder.Add(Checkpoint);
			Checkpoint->OnCheckpointReached.AddUniqueDynamic(this, &ARaceManager::HandleCheckpointReached);
			//UE_LOG(LogTemp, Log, TEXT("RaceManager: Registered Checkpoint %d (%s)"), Checkpoint->GetCheckpointOrder(), *Checkpoint->GetName());
			// No need to sort here immediately, do it once all are potentially registered.
		}
	}
}

void ARaceManager::SortCheckpoints()
{
	AllCheckpointsInOrder.Sort([](const ACheckpointTrigger& A, const ACheckpointTrigger& B) {
		return A.GetCheckpointOrder() < B.GetCheckpointOrder();
		});

	//UE_LOG(LogTemp, Log, TEXT("RaceManager: Sorted %d checkpoints."), AllCheckpointsInOrder.Num());
}

void ARaceManager::InitializeRaceSetup()
{
	if (!HasAuthority()) return;

	SortCheckpoints(); // Ensure they are in order first

	if (AllCheckpointsInOrder.Num() > 0)
	{
		bool bFoundStart = false;
		bool bFoundFinish = false;

		for (ACheckpointTrigger* CP : AllCheckpointsInOrder)
		{
			if (CP) // Ensure CP is valid
			{
				if (CP->GetCheckpointType() == ECheckpointType::Start && !bFoundStart)
				{
					StartLine = CP;
					bFoundStart = true;
					//UE_LOG(LogTemp, Log, TEXT("RaceManager: Start Line found: %s (Order: %d)"), *StartLine->GetName(), StartLine->GetCheckpointOrder());
				}
				else if (CP->GetCheckpointType() == ECheckpointType::Finish && !bFoundFinish)
				{
					FinishLine = CP;
					bFoundFinish = true;
					//UE_LOG(LogTemp, Log, TEXT("RaceManager: Finish Line found: %s (Order: %d)"), *FinishLine->GetName(), FinishLine->GetCheckpointOrder());
				}
			}
		}

		if (!bFoundStart && AllCheckpointsInOrder.Num() > 0)
		{
			// Fallback: if no explicit Start type, assume lowest order is start
			// But only if it's explicitly set as type Start OR it's the only one of type Checkpoint and order 0
			ACheckpointTrigger* PotentialStart = AllCheckpointsInOrder[0];
			if (PotentialStart->GetCheckpointType() == ECheckpointType::Start)
			{
				StartLine = PotentialStart;
				//UE_LOG(LogTemp, Warning, TEXT("RaceManager: Explicit Start Line found by order: %s"), *StartLine->GetName());
			}
			else
			{
				//UE_LOG(LogTemp, Error, TEXT("RaceManager: No checkpoint marked as 'Start'. Please mark one. Lowest order CP is %d"), AllCheckpointsInOrder[0]->GetCheckpointOrder());
			}
		}


		if (!bFoundFinish && AllCheckpointsInOrder.Num() > 1) // Need at least two (start and finish)
		{
			// Fallback: if no explicit Finish type, assume highest order is finish
			ACheckpointTrigger* PotentialFinish = AllCheckpointsInOrder.Last();
			if (PotentialFinish->GetCheckpointType() == ECheckpointType::Finish)
			{
				FinishLine = PotentialFinish;
				//UE_LOG(LogTemp, Warning, TEXT("RaceManager: Explicit Finish Line found by order: %s"), *FinishLine->GetName());
			}
			else
			{
				//UE_LOG(LogTemp, Error, TEXT("RaceManager: No checkpoint marked as 'Finish'. Please mark one. Highest order CP is %d"), AllCheckpointsInOrder.Last()->GetCheckpointOrder());
			}
		}

		if (StartLine && FinishLine && StartLine == FinishLine && AllCheckpointsInOrder.Num() > 1)
		{
			//UE_LOG(LogTemp, Error, TEXT("RaceManager: Start and Finish line cannot be the same checkpoint actor if there are other checkpoints."));
			// Potentially invalidate setup
		}


		TotalCheckpointsForFullLap = AllCheckpointsInOrder.Num();
		//UE_LOG(LogTemp, Log, TEXT("RaceManager: Initialization complete. Total Checkpoints for Lap (incl. Start/Finish): %d"), TotalCheckpointsForFullLap);

	}
	else
	{
		//UE_LOG(LogTemp, Warning, TEXT("RaceManager: No checkpoints found or registered during InitializeRaceSetup."));
	}
}


void ARaceManager::HandleCheckpointReached(ACheckpointTrigger* Checkpoint, AStrafeCharacter* PlayerCharacter)
{
	if (!HasAuthority() || !PlayerCharacter || !Checkpoint) // Simplified initial guard
	{
		UE_LOG(LogTemp, Warning, TEXT("ARaceManager::HandleCheckpointReached - Early exit: HasAuthority: %s, PlayerCharacter: %s, Checkpoint: %s"),
			HasAuthority() ? TEXT("true") : TEXT("false"),
			PlayerCharacter ? *PlayerCharacter->GetName() : TEXT("NULL"),
			Checkpoint ? *Checkpoint->GetName() : TEXT("NULL")
		);
		return;
	}

	// It's good practice to ensure StartLine and FinishLine are set up before processing race logic.
	// This might have been done in BeginPlay or an initialization function for RaceManager.
	// If they *could* be null here and that's an error, add a check. For now, assuming they are valid if race is active.
	if (!StartLine || !FinishLine)
	{
		UE_LOG(LogTemp, Error, TEXT("ARaceManager::HandleCheckpointReached - StartLine or FinishLine is not set in RaceManager!"));
		return;
	}

	APlayerState* PS = PlayerCharacter->GetPlayerState();
	if (!PS)
	{
		UE_LOG(LogTemp, Warning, TEXT("ARaceManager::HandleCheckpointReached - PlayerCharacter %s has no PlayerState."), *PlayerCharacter->GetName());
		return;
	}

	URaceStateComponent* RaceState = PS->FindComponentByClass<URaceStateComponent>();
	if (!RaceState)
	{
		UE_LOG(LogTemp, Warning, TEXT("ARaceManager::HandleCheckpointReached - PlayerState %s has no RaceStateComponent."), *PS->GetPlayerName());
		return;
	}

	const int32 CheckpointIdx = AllCheckpointsInOrder.IndexOfByKey(Checkpoint);
	if (CheckpointIdx == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("ARaceManager::HandleCheckpointReached - Reached checkpoint %s not in AllCheckpointsInOrder list!"), *Checkpoint->GetName());
		return;
	}

	// --- Start UE_LOGs for debugging values ---
	FString CheckpointTypeName = TEXT("Unknown");
	const UEnum* EnumPtr = StaticEnum<ECheckpointType>();
	if (EnumPtr)
	{
		CheckpointTypeName = EnumPtr->GetNameStringByValue(static_cast<int64>(Checkpoint->GetCheckpointType()));
	}

	UE_LOG(LogTemp, Warning, TEXT("ARaceManager: Player '%s' hit CP Name: '%s', CP Type: '%s' (EnumVal: %d), CP Index in AllCheckpointsInOrder: %d. RaceState LastCP Hit: %d, RaceState IsActive: %s, TotalCPsForLap (RaceManager): %d"),
		*PS->GetPlayerName(),
		*Checkpoint->GetName(),
		*CheckpointTypeName,
		static_cast<int32>(Checkpoint->GetCheckpointType()),
		CheckpointIdx,
		RaceState->GetLastCheckpointReached(),
		RaceState->IsRaceInProgress() ? TEXT("true") : TEXT("false"),
		TotalCheckpointsForFullLap
	);
	// --- End UE_LOGs ---

	if (Checkpoint->GetCheckpointType() == ECheckpointType::Start)
	{
		// If race is already active AND they are hitting the start line sequentially (e.g. completing a lap in a circuit)
		if (RaceState->IsRaceInProgress() && CheckpointIdx == RaceState->GetLastCheckpointReached() + 1)
		{
			UE_LOG(LogTemp, Log, TEXT("ARaceManager: Player %s is re-hitting Start Line sequentially (lap completed or reset). Resetting and starting new race."), *PS->GetPlayerName());
			RaceState->ResetRaceState(); // Reset previous run
			RaceState->StartRace();      // Start new one
			RaceState->ReachedCheckpoint(CheckpointIdx, TotalCheckpointsForFullLap); // Log the start line itself as the first checkpoint of the new run
		}
		// If race is not active, this is a fresh start
		else if (!RaceState->IsRaceInProgress())
		{
			UE_LOG(LogTemp, Log, TEXT("ARaceManager: Player %s starting race at Start Line %s."), *PS->GetPlayerName(), *Checkpoint->GetName());
			RaceState->StartRace();
			RaceState->ReachedCheckpoint(CheckpointIdx, TotalCheckpointsForFullLap); // Log the start line itself
		}
		else
		{
			// Race is in progress, but they hit Start out of sequence. Could be ignored or handled as a fault.
			UE_LOG(LogTemp, Warning, TEXT("ARaceManager: Player %s hit Start Line %s out of sequence. LastCP: %d, CPIdx: %d. No action taken."),
				*PS->GetPlayerName(), *Checkpoint->GetName(), RaceState->GetLastCheckpointReached(), CheckpointIdx);
		}
	}
	else if (Checkpoint->GetCheckpointType() == ECheckpointType::Finish)
	{
		if (RaceState->IsRaceInProgress())
		{
			UE_LOG(LogTemp, Log, TEXT("ARaceManager: Player %s hit Finish Line %s. Processing finish..."), *PS->GetPlayerName(), *Checkpoint->GetName());

			// ***** THE FIX: Call ReachedCheckpoint first for the finish line *****
			// This updates LastCheckpointReached and adds the final split time.
			RaceState->ReachedCheckpoint(CheckpointIdx, TotalCheckpointsForFullLap);

			// Now call FinishedRace. The conditions inside it should be met.
			RaceState->FinishedRace(CheckpointIdx, TotalCheckpointsForFullLap);
			// ***** END FIX *****

			UpdatePlayerInScoreboard(PS); // Update scoreboard on finish
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ARaceManager: Player %s hit Finish Line %s but race was not active. No action taken."), *PS->GetPlayerName(), *Checkpoint->GetName());
		}
	}
	else // ECheckpointType::Checkpoint (Intermediate)
	{
		if (RaceState->IsRaceInProgress())
		{
			UE_LOG(LogTemp, Log, TEXT("ARaceManager: Player %s hit Intermediate Checkpoint %s."), *PS->GetPlayerName(), *Checkpoint->GetName());
			RaceState->ReachedCheckpoint(CheckpointIdx, TotalCheckpointsForFullLap);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ARaceManager: Player %s hit Intermediate Checkpoint %s but race not active. Ignoring."), *PS->GetPlayerName(), *Checkpoint->GetName());
		}
	}
}

void ARaceManager::UpdatePlayerInScoreboard(APlayerState* PlayerState)
{
	if (!HasAuthority() || !PlayerState) return;

	URaceStateComponent* RaceState = PlayerState->FindComponentByClass<URaceStateComponent>();
	if (!RaceState) return;

	int32 EntryIndex = Scoreboard.IndexOfByPredicate([PlayerState](const FPlayerScoreboardEntry& Entry) {
		return Entry.PlayerStateRef == PlayerState;
		});

	if (EntryIndex != INDEX_NONE) // Existing entry
	{
		if (RaceState->GetBestRaceTime().TotalTime < Scoreboard[EntryIndex].BestTime.TotalTime)
		{
			Scoreboard[EntryIndex].BestTime = RaceState->GetBestRaceTime();
			//UE_LOG(LogTemp, Log, TEXT("Scoreboard: Updated best time for %s to %f"), *PlayerState->GetPlayerName(), Scoreboard[EntryIndex].BestTime.TotalTime);
		}
	}
	else // New entry
	{
		FPlayerScoreboardEntry NewEntry;
		NewEntry.PlayerName = PlayerState->GetPlayerName();
		NewEntry.BestTime = RaceState->GetBestRaceTime();
		NewEntry.PlayerStateRef = PlayerState;
		Scoreboard.Add(NewEntry);
		//UE_LOG(LogTemp, Log, TEXT("Scoreboard: Added %s with best time %f"), *NewEntry.PlayerName, NewEntry.BestTime.TotalTime);
	}

	// Sort scoreboard by best total time (ascending)
	Scoreboard.Sort([](const FPlayerScoreboardEntry& A, const FPlayerScoreboardEntry& B) {
		return A.BestTime.TotalTime < B.BestTime.TotalTime;
		});

	// This will trigger OnRep_Scoreboard for clients
	// Force a net update if needed, though DOREPLIFETIME should handle it.
	// GetWorld()->GetNetDriver()->ForcePropertyCompare(this, FindPropertyByName(GET_MEMBER_NAME_CHECKED(ARaceManager, Scoreboard)));
	OnRep_Scoreboard(); // Manually call for server
}


void ARaceManager::OnRep_Scoreboard()
{
	OnScoreboardUpdated.Broadcast();
	// GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("Scoreboard Replicated/Updated. Entries: %d"), Scoreboard.Num()));
	// if (Scoreboard.Num() > 0)
	// {
	//      GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("Top Score: %s - %.2f"), *Scoreboard[0].PlayerName, Scoreboard[0].BestTime.TotalTime));
	// }
}