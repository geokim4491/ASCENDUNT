#pragma once

#include "CoreMinimal.h"
#include "AscSegmentedHealthBarTypes.generated.h"

/** Logical health pool represented by a segmented health bar cell. */
UENUM(BlueprintType)
enum class EAscHealthPoolType : uint8
{
    Thorn,
    BasicHealth,
    Armor,
    OverHealth
};

/**
 * FriendlyDetailed exposes the health composition with pool-specific colors.
 * EnemyFlatRed hides the composition and renders the combined health as red cells.
 */
UENUM(BlueprintType)
enum class EAscSegmentedHealthBarVisualMode : uint8
{
    FriendlyDetailed,
    EnemyFlatRed
};
