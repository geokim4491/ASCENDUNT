#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/Mission/Conditions/AscObjectiveConditionComponent.h"
#include "AscInteractionConditionComponent.generated.h"

class AActor;
class AHeroCharacter;
class UWorld;

/**
 * Completion condition satisfied after every configured interactable target has
 * completed one authoritative interaction.
 *
 * Each target contributes at most once, even if it can be interacted with again.
 */
UCLASS(Blueprintable, ClassGroup=(Ascendunt), meta=(BlueprintSpawnableComponent))
class ASCENDUNT_API UAscInteractionConditionComponent : public UAscObjectiveConditionComponent
{
    GENERATED_BODY()

public:
    virtual float GetCurrentProgressValue_Implementation() const override;
    virtual float GetTargetProgressValue_Implementation() const override;

protected:
    virtual void OnActivated() override;
    virtual void OnDeactivated() override;
    virtual void OnReset() override;

    /**
     * Actors that must each complete one interaction.
     * The editor only accepts actors implementing IAscInteractable.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Condition|Interaction",
        meta = (MustImplement = "/Script/ASCENDUNT.AscInteractable"))
    TArray<TObjectPtr<AActor>> InteractionTargets;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Ascendunt|Mission|Condition|Interaction")
    int32 CurrentInteractionCount = 0;

private:
    void HandleInteractionCompleted(
        UWorld* InteractionWorld,
        AActor* Target,
        AHeroCharacter* Interactor);

    int32 GetRequiredInteractionCount() const;
    void BindInteractionEvent();
    void UnbindInteractionEvent();

    /** Prevents one interactable from incrementing the condition more than once. */
    TSet<TObjectPtr<AActor>> CompletedTargets;
};
