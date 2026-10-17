#pragma once

#include "CoreMinimal.h"
#include "AscScoreboardTypes.generated.h"

class AAscenduntPlayerState;
class UTexture2D;

/** One replicated player's snapshot as consumed by the local TAB scoreboard. */
USTRUCT(BlueprintType)
struct ASCENDUNT_API FAscScoreboardPlayerEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Scoreboard")
    TObjectPtr<AAscenduntPlayerState> PlayerState = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Scoreboard")
    FText PlayerName;

    UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Scoreboard")
    int32 HeroId = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Scoreboard")
    TObjectPtr<UTexture2D> Portrait = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Scoreboard")
    int32 MonsterKills = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Scoreboard")
    int32 DownedCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Scoreboard")
    int32 ReviveCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Scoreboard")
    float DamageDealt = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Scoreboard")
    float HealingDone = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Scoreboard")
    float HealingReceived = 0.0f;

    /** Normalized 0..1. */
    UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Scoreboard")
    float UltimateChargePercent = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Scoreboard")
    bool bIsLocalPlayer = false;
};

/** Extra stats shown only in the local player's right-side detail panel. */
USTRUCT(BlueprintType)
struct ASCENDUNT_API FAscScoreboardLocalWeaponStats
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Scoreboard")
    int32 WeaponShotsFired = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Scoreboard")
    int32 WeaponHits = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Scoreboard")
    int32 CriticalWeaponHits = 0;

    /** 0..100. */
    UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Scoreboard")
    float WeaponAccuracyPercent = 0.0f;

    /** Critical hits / weapon hits, 0..100. */
    UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Scoreboard")
    float CriticalHitAccuracyPercent = 0.0f;
};
