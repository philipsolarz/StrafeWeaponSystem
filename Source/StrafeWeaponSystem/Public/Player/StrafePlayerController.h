#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "StrafePlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UUserWidget;

UCLASS()
class STRAFEWEAPONSYSTEM_API AStrafePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AStrafePlayerController();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|UI")
	UInputMappingContext* ScoreboardMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|UI|Actions")
	UInputAction* ToggleScoreboardAction;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> ScoreboardWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> ScoreboardWidgetInstance;

	bool bIsScoreboardVisible;

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	UFUNCTION()
	void ToggleScoreboard();

	/** Override to ensure UI input can be processed when scoreboard is up */
	virtual void SetInputMode(FInputModeDataBase const& InData) override;

public:
	UFUNCTION(BlueprintCallable, Category = "UI")
	bool IsScoreboardVisible() const { return bIsScoreboardVisible; }
};