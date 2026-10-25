#pragma once

#include "CoreMinimal.h"
#include "AscInteractionTypes.generated.h"

/** How the common Interact input should activate this target. */
UENUM(BlueprintType)
enum class EAscInteractionInputMode : uint8
{
    Pressed UMETA(DisplayName = "Pressed"),
    Hold    UMETA(DisplayName = "Hold")
};

/**
 * Runtime interaction parameters supplied by an interactable target.
 *
 * GA_Interact owns the input lifecycle. Targets only describe how they should
 * be interacted with and receive begin/cancel/complete callbacks.
 */
USTRUCT(BlueprintType)
struct ASCENDUNT_API FAscInteractionOption
{
    GENERATED_BODY()

public:
    /** Text displayed after the common interaction key prompt (ex. "부활", "전력 복구"). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Interaction")
    FText PromptText;

    /** Pressed completes on input press. Hold requires keeping Interact pressed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Interaction")
    EAscInteractionInputMode InputMode = EAscInteractionInputMode::Pressed;

    /** Used only when InputMode == Hold. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Interaction",
        meta = (ClampMin = "0.01", EditCondition = "InputMode == EAscInteractionInputMode::Hold", EditConditionHides))
    float HoldDuration = 1.0f;

    /** Maximum interaction distance. Measured to the target's closest query-collision surface when possible. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Interaction", meta = (ClampMin = "1.0"))
    float MaxDistance = 250.0f;

    /** If true, GA_Interact temporarily ignores movement input while an interaction is active. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Interaction")
    bool bLockMovement = false;

    /** Higher-priority targets win when multiple valid interactables are inside the interaction FOV. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Interaction")
    float Priority = 0.0f;
};
