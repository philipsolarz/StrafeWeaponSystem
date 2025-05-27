// New file: Source/StrafeWeaponSystem/Private/Weapons/ChargedShotgun.cpp
#include "Weapons/ChargedShotgun.h"
#include "WeaponDataAsset.h"
#include "StrafeCharacter.h" // For ability system component access for cues
#include "AbilitySystemBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "GameFramework/DamageType.h"
#include "AbilitySystemComponent.h"

AChargedShotgun::AChargedShotgun()
{
    // Default values specific to this weapon type, if any, can be set here.
    // Most configuration will come from the UWeaponDataAsset.
}

void AChargedShotgun::BeginPlay()
{
    Super::BeginPlay();
}

void AChargedShotgun::PerformHitscanShot(
    const FVector& StartLocation,
    const FVector& AimDirection,
    int32 PelletCount,
    float SpreadAngle,
    float HitscanRange,
    float DamageToApply,
    TSubclassOf<class UDamageType> DamageTypeClass,
    APawn* InstigatorPawn,
    AController* InstigatorController,
    FGameplayTag OptionalImpactCueTag)
{
    UE_LOG(LogTemp, Log, TEXT("AChargedShotgun::PerformHitscanShot - PelletCount: %d, Spread: %f, Range: %f"),
        PelletCount, SpreadAngle, HitscanRange);

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("AChargedShotgun::PerformHitscanShot - No World!"));
        return;
    }

    if (!InstigatorPawn || !InstigatorController)
    {
        UE_LOG(LogTemp, Warning, TEXT("AChargedShotgun::PerformHitscanShot - Missing InstigatorPawn or Controller"));
        return;
    }

    UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InstigatorPawn);

    for (int32 i = 0; i < PelletCount; ++i)
    {
        // Calculate spread for each pellet
        // This is a common way to do it: random point in a circle perpendicular to aim, then project.
        // For simplicity here, we'll use a simpler cone spread.
        const float HalfAngleRad = FMath::DegreesToRadians(SpreadAngle * 0.5f);
        const FVector PelletDir = FMath::VRandCone(AimDirection, HalfAngleRad);

        FVector TraceStart = StartLocation;
        FVector TraceEnd = TraceStart + (PelletDir * HitscanRange);

        FHitResult HitResult;
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(this); // Ignore the weapon itself
        QueryParams.AddIgnoredActor(InstigatorPawn); // Ignore the firer
        QueryParams.bReturnPhysicalMaterial = true; // Useful for varied impact effects

        bool bHit = GetWorld()->LineTraceSingleByChannel(
            HitResult,
            TraceStart,
            TraceEnd,
            ECC_Visibility, // Or a custom trace channel for projectiles/weapon fire
            QueryParams
        );

        FVector EndPoint = bHit ? HitResult.ImpactPoint : TraceEnd;

        // Play impact effect using Gameplay Cue
        if (OptionalImpactCueTag.IsValid() && SourceASC)
        {
            FGameplayCueParameters CueParams;
            CueParams.Location = EndPoint;
            CueParams.Normal = HitResult.ImpactNormal; // If bHit is true, otherwise AimDirection
            CueParams.PhysicalMaterial = HitResult.PhysMaterial;
            CueParams.Instigator = InstigatorPawn;
            CueParams.EffectContext = SourceASC->MakeEffectContext();
            CueParams.EffectContext.AddSourceObject(this);

            // Differentiate between surface hit and no-hit for the cue if needed
            // For example, by adding a GameplayTag to CueParams.AggregatedSourceTags or TargetTags
            if (bHit)
            {
                CueParams.TargetAttachComponent = HitResult.GetComponent(); // Attach to what was hit
            }
            SourceASC->ExecuteGameplayCue(OptionalImpactCueTag, CueParams);
        }

        // Apply damage if something was hit
        if (bHit && HitResult.GetActor())
        {
            UGameplayStatics::ApplyPointDamage(
                HitResult.GetActor(),
                DamageToApply,
                PelletDir, // Direction of this pellet
                HitResult,
                InstigatorController,
                this, // Damage causer (the weapon)
                DamageTypeClass
            );
        }

        // Debug drawing (optional, remove for release)
        // Note: On dedicated server, DrawDebugLine might not be visible.
        // Wrap in #if WITH_EDITOR || ENABLE_DRAW_DEBUG
        // For multiplayer, consider using a replicated debug draw system if needed.
        if (GetNetMode() != NM_DedicatedServer) // Only draw on clients/listen server
        {
            DrawDebugLine(GetWorld(), TraceStart, EndPoint, FColor::Red, false, 1.0f, 0, 0.5f);
            if (bHit) DrawDebugSphere(GetWorld(), HitResult.ImpactPoint, 5.f, 8, FColor::Yellow, false, 1.0f);
        }
    }
}