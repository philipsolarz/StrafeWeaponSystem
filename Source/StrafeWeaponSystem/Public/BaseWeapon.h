// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponDataAsset.h"
#include "GameplayTagContainer.h"
#include "BaseWeapon.generated.h"

class AProjectileBase;
class USkeletalMeshComponent;

UCLASS(Abstract)
class STRAFEWEAPONSYSTEM_API ABaseWeapon : public AActor
{
    GENERATED_BODY()

public:
    ABaseWeapon();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USkeletalMeshComponent* WeaponMesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Attachment")
    FName AttachSocketName = "WeaponSocket";

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    UWeaponDataAsset* WeaponData;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon")
    bool  bIsEquipped = false;

    UPROPERTY(BlueprintReadOnly, Category = "Weapon")
    TArray<AProjectileBase*> ActiveProjectiles;


public:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category = "Weapon")
    UWeaponDataAsset* GetWeaponData() const { return WeaponData; }

    UFUNCTION(BlueprintPure, Category = "Components")
    USkeletalMeshComponent* GetWeaponMeshComponent() const { return WeaponMesh; }

    UFUNCTION(BlueprintCallable, Category = "Weapon") virtual void Equip(ACharacter* NewOwner);
    UFUNCTION(BlueprintCallable, Category = "Weapon") virtual void Unequip();

    bool IsEquipped() const { return bIsEquipped; }

public:
    void RegisterProjectile(AProjectileBase* Projectile);
    void UnregisterProjectile(AProjectileBase* Projectile);

    UFUNCTION(BlueprintPure, Category = "Weapon")
    const TArray<AProjectileBase*>& GetActiveProjectiles() const { return ActiveProjectiles; }
protected:
    UFUNCTION() void OnProjectileDestroyed(AActor* DestroyedActor);

private:
    friend class AProjectileBase;
};