#include "Race/CheckpointTrigger.h"
#include "Components/BoxComponent.h"
#include "Components/BillboardComponent.h"
#include "StrafeCharacter.h" // Assuming your character class is AStrafeCharacter
#include "Kismet/GameplayStatics.h"
#include "Player/RaceStateComponent.h" // We will create this next

ACheckpointTrigger::ACheckpointTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerVolume"));
	RootComponent = TriggerVolume;
	TriggerVolume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	TriggerVolume->OnComponentBeginOverlap.AddDynamic(this, &ACheckpointTrigger::OnTriggerOverlap);
	TriggerVolume->SetCanEverAffectNavigation(false);

	EditorBillboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("EditorBillboard"));
	if (EditorBillboard)
	{
		EditorBillboard->SetupAttachment(RootComponent);
	}


	CheckpointOrder = 0;
	TypeOfCheckpoint = ECheckpointType::Checkpoint;
	bReplicates = true; // Important for server to reliably detect overlaps
	SetReplicatingMovement(false);
}

void ACheckpointTrigger::BeginPlay()
{
	Super::BeginPlay();
}

void ACheckpointTrigger::OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (HasAuthority()) // Only server should process checkpoint logic
	{
		AStrafeCharacter* PlayerCharacter = Cast<AStrafeCharacter>(OtherActor);
		if (PlayerCharacter)
		{
			//UE_LOG(LogTemp, Warning, TEXT("Checkpoint %d overlapped by %s"), CheckpointOrder, *PlayerCharacter->GetName());
			OnCheckpointReached.Broadcast(this, PlayerCharacter);
		}
	}
}