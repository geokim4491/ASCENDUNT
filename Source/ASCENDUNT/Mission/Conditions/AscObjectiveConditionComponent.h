#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ASCENDUNT/Mission/Objective/AscObjectiveTypes.h"
#include "AscObjectiveConditionComponent.generated.h"

class AAscObjectiveBase;
class UAscObjectiveConditionComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FAscConditionComponentSimpleEvent,
    UAscObjectiveConditionComponent*, Condition);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FAscConditionComponentStateChangedEvent,
    UAscObjectiveConditionComponent*, Condition,
    EAscObjectiveConditionState, OldState,
    EAscObjectiveConditionState, NewState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FAscConditionComponentProgressChangedEvent,
    UAscObjectiveConditionComponent*, Condition,
    float, CurrentValue,
    float, TargetValue);

/**
 * Atomic mission condition attached to an AAscObjectiveBase as an ActorComponent.
 *
 * The component owns only its own condition state. The objective decides whether
 * Satisfied means objective completion or failure based on ConditionRole.
 */
UCLASS(Abstract, Blueprintable, ClassGroup=(Ascendunt), meta=(BlueprintSpawnableComponent))
class ASCENDUNT_API UAscObjectiveConditionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAscObjectiveConditionComponent();

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Mission|Condition")
    virtual void ActivateCondition();

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Mission|Condition")
    virtual void DeactivateCondition();

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Mission|Condition")
    virtual void ResetCondition();

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission|Condition")
    FGameplayTag GetConditionId() const { return ConditionId; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission|Condition")
    EAscObjectiveConditionRole GetConditionRole() const { return ConditionRole; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission|Condition|UI")
    EAscObjectiveProgressDisplayType GetProgressDisplayType() const { return ProgressDisplayType; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission|Condition|UI")
    FText GetProgressLabel() const { return ProgressLabel; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission|Condition")
    EAscObjectiveConditionState GetConditionState() const { return State; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission|Condition")
    bool IsConditionActive() const { return State == EAscObjectiveConditionState::Active; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission|Condition")
    bool IsConditionSatisfied() const { return State == EAscObjectiveConditionState::Satisfied; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission|Condition")
    bool IsConditionFailed() const { return State == EAscObjectiveConditionState::Failed; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission|Condition")
    AAscObjectiveBase* GetOwningObjective() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ascendunt|Mission|Condition")
    float GetCurrentProgressValue() const;
    virtual float GetCurrentProgressValue_Implementation() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ascendunt|Mission|Condition")
    float GetTargetProgressValue() const;
    virtual float GetTargetProgressValue_Implementation() const;

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Mission|Condition")
    FAscConditionComponentSimpleEvent OnConditionActivated;

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Mission|Condition")
    FAscConditionComponentSimpleEvent OnConditionSatisfied;

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Mission|Condition")
    FAscConditionComponentSimpleEvent OnConditionFailed;

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Mission|Condition")
    FAscConditionComponentStateChangedEvent OnConditionStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Mission|Condition")
    FAscConditionComponentProgressChangedEvent OnConditionProgressChanged;

    /**
     * Called by AAscObjectiveBase on clients when a replicated condition snapshot changes.
     * This keeps direct BP bindings on the component usable without replicating each
     * condition component independently.
     */
    void ApplyReplicatedSnapshot(const FAscObjectiveConditionSnapshot& Snapshot);

protected:
    /** Must be unique among the condition components on one objective. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Condition")
    FGameplayTag ConditionId;

    /** Completion conditions contribute to success; Failure conditions contribute to failure. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Condition")
    EAscObjectiveConditionRole ConditionRole = EAscObjectiveConditionRole::Completion;

    /** Controls only how this condition is presented by mission UI. It does not affect mission logic. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Condition|UI")
    EAscObjectiveProgressDisplayType ProgressDisplayType = EAscObjectiveProgressDisplayType::Hidden;

    /** Optional player-facing label such as POWER or TARGETING LINK. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Condition|UI")
    FText ProgressLabel;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Ascendunt|Mission|Condition")
    EAscObjectiveConditionState State = EAscObjectiveConditionState::Inactive;

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Mission|Condition", meta = (BlueprintProtected = "true"))
    void Satisfy();

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Mission|Condition", meta = (BlueprintProtected = "true"))
    void Fail();

    void NotifyProgressChanged();

    virtual void OnActivated() {}
    virtual void OnDeactivated() {}
    virtual void OnReset() {}

private:
    void SetState(EAscObjectiveConditionState NewState);

    float LastReplicatedCurrentValue = 0.0f;
    float LastReplicatedTargetValue = 1.0f;
};
