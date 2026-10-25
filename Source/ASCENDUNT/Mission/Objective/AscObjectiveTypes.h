#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AscObjectiveTypes.generated.h"

UENUM(BlueprintType)
enum class EAscObjectiveState : uint8
{
    Inactive,
    Active,
    Completed,
    Failed
};

UENUM(BlueprintType)
enum class EAscObjectiveConditionState : uint8
{
    Inactive,
    Active,
    Satisfied,
    Failed
};

UENUM(BlueprintType)
enum class EAscObjectiveConditionPolicy : uint8
{
    All,
    Any
};

UENUM(BlueprintType)
enum class EAscObjectiveProgressDisplayType : uint8
{
    Hidden,
    Count,
    Percent
};

/**
 * How a condition participates in its owning objective.
 * Completion: satisfying the condition contributes to objective success.
 * Failure: satisfying the condition contributes to objective failure.
 */
UENUM(BlueprintType)
enum class EAscObjectiveConditionRole : uint8
{
    Completion,
    Failure
};

USTRUCT(BlueprintType)
struct ASCENDUNT_API FAscObjectiveConditionSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Mission")
    FGameplayTag ConditionId;

    UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Mission")
    EAscObjectiveConditionState State = EAscObjectiveConditionState::Inactive;

    UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Mission")
    float CurrentValue = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Mission")
    float TargetValue = 1.0f;
};
