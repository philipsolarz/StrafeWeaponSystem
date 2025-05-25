// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BaseWeaponPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class ABaseWeapon;
class AStrafeCharacter;

UCLASS()
class STRAFEWEAPONSYSTEM_API ABaseWeaponPickup : public AActor
{
    GENERATED_BODY()

public:
    ABaseWeaponPickup();

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* CollisionSphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* PickupMesh; // Optional visual representation

    // The class of weapon this pickup will grant
    UPROPERTY(EditDefaultsOnly, Category = "Pickup")
    TSubclassOf<ABaseWeapon> WeaponClassToGrant;

    UPROPERTY(EditDefaultsOnly, Category = "Pickup")
    bool bDestroyOnPickup;

    // Cooldown before the pickup can be taken again (if not destroyed)
    UPROPERTY(EditDefaultsOnly, Category = "Pickup", meta = (EditCondition = "!bDestroyOnPickup"))
    float RespawnTime;

    FTimerHandle RespawnTimerHandle;

    UPROPERTY(ReplicatedUsing = OnRep_IsActive)
    bool bIsActive;

    UFUNCTION()
    void OnRep_IsActive();

    // Use component overlap for more reliable detection
    UFUNCTION()
    void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

    UFUNCTION() // Server-side logic
        void ProcessPickup(AStrafeCharacter* PickingCharacter);

    UFUNCTION(BlueprintImplementableEvent, Category = "Pickup")
    void OnPickedUpEffectsBP(); // For sounds/VFX in Blueprint

    UFUNCTION(BlueprintImplementableEvent, Category = "Pickup")
    void OnRespawnEffectsBP(); // For sounds/VFX in Blueprint

    UFUNCTION(NetMulticast, Reliable) // For effects that need to be precisely timed on all clients
        void Multicast_OnPickedUpEffects();

    void AttemptRespawn();
    void SetPickupActiveState(bool bNewActiveState); // Server sets this

public:
    virtual void Tick(float DeltaTime) override; // Only if needed
};