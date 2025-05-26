#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheckpointTrigger.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCheckpointReached, class ACheckpointTrigger*, Checkpoint, class AStrafeCharacter*, PlayerCharacter);

UENUM(BlueprintType)
enum class ECheckpointType : uint8
{
	Start UMETA(DisplayName = "Start Line"),
	Checkpoint UMETA(DisplayName = "Checkpoint"),
	Finish UMETA(DisplayName = "Finish Line")
};

UCLASS(Blueprintable, BlueprintType)
class STRAFEWEAPONSYSTEM_API ACheckpointTrigger : public AActor
{
	GENERATED_BODY()

public:
	ACheckpointTrigger();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UBoxComponent* TriggerVolume;

	// Order of this checkpoint in the race sequence. Start is typically 0, Finish is the highest.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race")
	int32 CheckpointOrder;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race")
	ECheckpointType TypeOfCheckpoint;

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	UPROPERTY(BlueprintAssignable, Category = "Race")
	FOnCheckpointReached OnCheckpointReached;

	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION(BlueprintPure, Category = "Race")
	int32 GetCheckpointOrder() const { return CheckpointOrder; }

	UFUNCTION(BlueprintPure, Category = "Race")
	ECheckpointType GetCheckpointType() const { return TypeOfCheckpoint; }

	// Optional: Visual component for designers to see in editor
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UBillboardComponent* EditorBillboard;
};