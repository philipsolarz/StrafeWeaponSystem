// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h" // Required for TSubclassOf<UGameplayEffect>
#include "BaseWeaponPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class ABaseWeapon;
class AStrafeCharacter;
// class UGameplayEffect; // Forward declared by GameplayEffect.h

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
    UStaticMeshComponent* PickupMesh;

    // The class of weapon this pickup will grant if the player doesn't have it.
    UPROPERTY(EditDefaultsOnly, Category = "Pickup")
    TSubclassOf<ABaseWeapon> WeaponClassToGrant;

    // GameplayEffect to apply when this pickup is collected.
    // - If it's a weapon pickup and player doesn't have it, AddWeapon is called first.
    //   Then this GE could grant initial ammo (e.g. using WeaponData's InitialAmmoCount for the specific weapon).
    // - If it's an ammo-only pickup, this GE would directly modify the relevant ammo attribute.
    // - If it's a weapon pickup for an existing weapon, this GE could grant additional ammo.
    UPROPERTY(EditDefaultsOnly, Category = "Pickup|GAS")
    TSubclassOf<UGameplayEffect> PickupGameplayEffect;

    UPROPERTY(EditDefaultsOnly, Category = "Pickup")
    bool bDestroyOnPickup;

    UPROPERTY(EditDefaultsOnly, Category = "Pickup", meta = (EditCondition = "!bDestroyOnPickup"))
    float RespawnTime;

    FTimerHandle RespawnTimerHandle;

    UPROPERTY(ReplicatedUsing = OnRep_IsActive)
    bool bIsActive;

    UFUNCTION()
    void OnRep_IsActive();

    UFUNCTION()
    void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    // NotifyActorBeginOverlap might still be useful as a fallback or for different interaction types.
    // virtual void NotifyActorBeginOverlap(AActor* OtherActor) override; 

    UFUNCTION()
    void ProcessPickup(AStrafeCharacter* PickingCharacter);

    UFUNCTION(BlueprintImplementableEvent, Category = "Pickup")
    void OnPickedUpEffectsBP();

    UFUNCTION(BlueprintImplementableEvent, Category = "Pickup")
    void OnRespawnEffectsBP();

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_OnPickedUpEffects();

    void AttemptRespawn();
    void SetPickupActiveState(bool bNewActiveState);

public:
    // virtual void Tick(float DeltaTime) override; // Only if needed
};