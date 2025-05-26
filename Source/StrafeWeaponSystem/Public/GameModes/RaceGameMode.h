#pragma once

#include "CoreMinimal.h"
#include "StrafeGameMode.h" // Assuming you have a base StrafeGameMode, otherwise GameModeBase or GameMode
#include "Player/StrafePlayerController.h"
#include "RaceGameMode.generated.h"

class ARaceManager;

UCLASS()
class STRAFEWEAPONSYSTEM_API ARaceGameMode : public AStrafeGameMode // Or your relevant base game mode
{
	GENERATED_BODY()

public:
	ARaceGameMode();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Race")
	TSubclassOf<ARaceManager> RaceManagerClass;

	UPROPERTY()
	ARaceManager* CurrentRaceManager;

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void InitGameState() override; // Good place to ensure RaceManager is known to GameState

public:
	UFUNCTION(BlueprintPure, Category = "Race")
	ARaceManager* GetRaceManager() const { return CurrentRaceManager; }
};