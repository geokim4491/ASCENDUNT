#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AscPvEDownedConfig.generated.h"

class UGameplayEffect;

/**
 * Shared PvE Downed/Revive rules for every hero.
 *
 * Runtime state stays in UAscPvEDownedComponent; this asset only owns common tuning/assets.
 * AHeroCharacter loads the single project-wide asset from a fixed content path and injects it
 * into its DownedComponent, so individual Hero Blueprints do not configure these values.
 */
UCLASS(BlueprintType, Const)
class ASCENDUNT_API UAscPvEDownedConfig : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /** Seconds from entering Downed until full death. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Downed", meta = (ClampMin = "0.1"))
    float BleedoutDuration = 15.0f;

    /** CharacterMovement MaxWalkSpeed while crawling in Downed. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Downed", meta = (ClampMin = "0.0"))
    float DownedCrawlSpeed = 120.0f;

    /** Seconds V must be held to complete a revive. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Revive", meta = (ClampMin = "0.1"))
    float ReviveHoldDuration = 3.0f;

    /** Maximum interaction distance for revive. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Revive", meta = (ClampMin = "1.0"))
    float ReviveMaxDistance = 250.0f;

    /** Revive amount = (MaxBasicHealth + MaxArmor + MaxThorn) * this value. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Revive", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float ReviveHealthFraction = 1.0f / 3.0f;

    /** Server-authoritative invulnerability after successful revive. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Revive", meta = (ClampMin = "0.0"))
    float ReviveGraceDuration = 1.0f;

    /**
     * Instant healing GE using UAscenduntHealExecution.
     * Runtime code injects Data.HealOverride and Effect.Revive into the Spec.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Revive")
    TSubclassOf<UGameplayEffect> ReviveHealEffectClass;
};
