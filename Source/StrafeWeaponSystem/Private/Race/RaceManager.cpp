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
	if (!HasAuthority() || !PlayerCharacter || !Checkpoint || !StartLine || !FinishLine)
	{
		//UE_LOG(LogTemp, Warning, TEXT("RaceManager::HandleCheckpointReached - Invalid state or params. HasAuth: %d, Player: %s, CP: %s, Start: %s, Finish: %s"),
		//	HasAuthority(), PlayerCharacter ? TEXT("Valid") : TEXT("NULL"), Checkpoint ? TEXT("Valid") : TEXT("NULL"), StartLine ? TEXT("Valid") : TEXT("NULL"), FinishLine ? TEXT("Valid") : TEXT("NULL"));
		return;
	}

	APlayerState* PS = PlayerCharacter->GetPlayerState();
	if (!PS)
	{
		//UE_LOG(LogTemp, Warning, TEXT("RaceManager::HandleCheckpointReached - PlayerCharacter has no PlayerState."));
		return;
	}

	URaceStateComponent* RaceState = PS->FindComponentByClass<URaceStateComponent>();
	if (!RaceState)
	{
		//UE_LOG(LogTemp, Warning, TEXT("RaceManager::HandleCheckpointReached - PlayerState %s has no RaceStateComponent."), *PS->GetPlayerName());
		return;
	}

	const int32 CheckpointIdx = AllCheckpointsInOrder.IndexOfByKey(Checkpoint);
	if (CheckpointIdx == INDEX_NONE)
	{
		//UE_LOG(LogTemp, Error, TEXT("RaceManager::HandleCheckpointReached - Reached checkpoint %s not in known list!"), *Checkpoint->GetName());
		return;
	}

	if (Checkpoint->GetCheckpointType() == ECheckpointType::Start)
	{
		if (RaceState->IsRaceInProgress() && CheckpointIdx == RaceState->GetLastCheckpointReached() + 1)
		{
			// This means they lapped AND correctly hit the start line again as the next checkpoint
			// For a simple A->C race, this might mean they finished and are restarting.
			// For a circuit, this would be a new lap.
			// For now, let's assume A->C. If they hit start again while race is active, it's like a reset for this model.
			RaceState->ResetRaceState(); // Reset previous run
			RaceState->StartRace();      // Start new one
			RaceState->ReachedCheckpoint(CheckpointIdx, TotalCheckpointsForFullLap); // Log the start line itself as first CP
		}
		else if (!RaceState->IsRaceInProgress())
		{
			RaceState->StartRace();
			RaceState->ReachedCheckpoint(CheckpointIdx, TotalCheckpointsForFullLap); // Log the start line itself
		}
	}
	else if (Checkpoint->GetCheckpointType() == ECheckpointType::Finish)
	{
		if (RaceState->IsRaceInProgress())
		{
			RaceState->FinishedRace(CheckpointIdx, TotalCheckpointsForFullLap);
			UpdatePlayerInScoreboard(PS); // Update scoreboard on finish
		}
	}
	else // Regular checkpoint
	{
		if (RaceState->IsRaceInProgress())
		{
			RaceState->ReachedCheckpoint(CheckpointIdx, TotalCheckpointsForFullLap);
		}
		else
		{
			// Hit a mid-checkpoint without starting the race (e.g. joined mid-game, teleported)
			// For robustness, you might want to start their race here if they hit a non-start CP first.
			// Or, simply ignore it if strict start-to-finish is required.
			// UE_LOG(LogTemp, Warning, TEXT("%s hit checkpoint %s but race not active. Ignoring."), *PS->GetPlayerName(), *Checkpoint->GetName());
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