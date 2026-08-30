#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "ASCENDUNT/Mission/Objective/AscObjectiveTypes.h"
#include "AscObjectiveBase.generated.h"

class AAscObjectiveBase;
class UAscObjectiveConditionComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FAscObjectiveSimpleEvent,
    AAscObjectiveBase*, Objective);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FAscObjectiveStateChangedEvent,
    AAscObjectiveBase*, Objective,
    EAscObjectiveState, OldState,
    EAscObjectiveState, NewState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FAscObjectiveConditionEvent,
    AAscObjectiveBase*, Objective,
    FGameplayTag, ConditionId);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
    FAscObjectiveConditionStateEvent,
    AAscObjectiveBase*, Objective,
    FGameplayTag, ConditionId,
    EAscObjectiveConditionState, OldState,
    EAscObjectiveConditionState, NewState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
    FAscObjectiveConditionProgressEvent,
    AAscObjectiveBase*, Objective,
    FGameplayTag, ConditionId,
    float, CurrentValue,
    float, TargetValue);

/**
 * One player-facing objective.
 *
 * Reusable condition ActorComponents are attached to this Actor in Blueprint.
 * The objective discovers them, splits them by ConditionRole, and evaluates
 * Completion/Failure with the configured All/Any policies.
 */
UCLASS(Blueprintable)
class ASCENDUNT_API AAscObjectiveBase : public AActor
{
    GENERATED_BODY()

public:
    AAscObjectiveBase();

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Mission|Objective")
    void ActivateObjective();

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Mission|Objective")
    void DeactivateObjective();

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Mission|Objective")
    void ResetObjective();

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission|Objective")
    FGameplayTag GetObjectiveId() const { return ObjectiveId; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission|Objective")
    FText GetObjectiveTitle() const { return ObjectiveTitle; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission|Objective")
    FText GetObjectiveDescription() const { return ObjectiveDescription; }

    /** Delay from Objective activation to the temporary top-center title announcement. */
    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission|Objective|UI")
    float GetObjectiveTitleDisplayDelay() const { return ObjectiveTitleDisplayDelay; }

    /** Duration for which the temporary top-center ObjectiveTitle stays visible. */
    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission|Objective|UI")
    float GetObjectiveTitleDisplayDuration() const { return ObjectiveTitleDisplayDuration; }

    /** Optional condition explicitly selected for player-facing progress UI. */
    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission|Objective|UI")
    FGameplayTag GetProgressConditionId() const { return ProgressConditionId; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission|Objective")
    EAscObjectiveState GetObjectiveState() const { return State; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission|Objective")
    bool IsActive() const { return State == EAscObjectiveState::Active; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission|Objective")
    bool IsCompleted() const { return State == EAscObjectiveState::Completed; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission|Objective")
    bool IsFailed() const { return State == EAscObjectiveState::Failed; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission|Objective")
    TArray<FAscObjectiveConditionSnapshot> GetConditionSnapshots() const { return ConditionSnapshots; }

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Mission|Objective")
    bool GetConditionSnapshot(FGameplayTag ConditionId, FAscObjectiveConditionSnapshot& OutSnapshot) const;

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission|Objective")
    UAscObjectiveConditionComponent* GetConditionComponent(FGameplayTag ConditionId) const;

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Mission|Objective")
    FAscObjectiveSimpleEvent OnObjectiveActivated;

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Mission|Objective")
    FAscObjectiveSimpleEvent OnObjectiveCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Mission|Objective")
    FAscObjectiveSimpleEvent OnObjectiveFailed;

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Mission|Objective")
    FAscObjectiveStateChangedEvent OnObjectiveStateChanged;

    /** Forwarded condition events. External BP can bind here by ConditionId. */
    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Mission|Objective|Condition")
    FAscObjectiveConditionEvent OnConditionSatisfied;

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Mission|Objective|Condition")
    FAscObjectiveConditionEvent OnConditionFailed;

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Mission|Objective|Condition")
    FAscObjectiveConditionStateEvent OnConditionStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Mission|Objective|Condition")
    FAscObjectiveConditionProgressEvent OnConditionProgressChanged;

protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Objective")
    FGameplayTag ObjectiveId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Objective")
    FText ObjectiveTitle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Objective", meta = (MultiLine = "true"))
    FText ObjectiveDescription;

    /** BP-configurable delay after activation before ObjectiveTitle is presented at top center. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Objective|UI", meta = (ClampMin = "0"))
    float ObjectiveTitleDisplayDelay = 0.0f;

    /** How long the temporary top-center ObjectiveTitle stays visible after it is presented. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Objective|UI", meta = (ClampMin = "0"))
    float ObjectiveTitleDisplayDuration = 3.0f;

    /** If valid, mission UI uses this condition for progress. If empty, the first visible condition is used. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Objective|UI")
    FGameplayTag ProgressConditionId;

    /** Evaluates only components whose ConditionRole is Completion. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Objective|Completion")
    EAscObjectiveConditionPolicy CompletionPolicy = EAscObjectiveConditionPolicy::All;

    /** Evaluates only components whose ConditionRole is Failure. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Objective|Failure")
    EAscObjectiveConditionPolicy FailurePolicy = EAscObjectiveConditionPolicy::Any;

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Mission|Objective", meta = (BlueprintProtected = "true"))
    void CompleteObjective();

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Mission|Objective", meta = (BlueprintProtected = "true"))
    void FailObjective();

private:
    UFUNCTION()
    void HandleConditionStateChanged(
        UAscObjectiveConditionComponent* Condition,
        EAscObjectiveConditionState OldState,
        EAscObjectiveConditionState NewState);

    UFUNCTION()
    void HandleConditionProgressChanged(
        UAscObjectiveConditionComponent* Condition,
        float CurrentValue,
        float TargetValue);

    UFUNCTION()
    void OnRep_State(EAscObjectiveState OldState);

    UFUNCTION()
    void OnRep_ConditionSnapshots();

    void InitializeConditionComponents();
    void ActivateConditions();
    void DeactivateConditions();
    void EvaluateObjective();

    bool AreConditionsSatisfied(
        const TArray<TObjectPtr<UAscObjectiveConditionComponent>>& Conditions,
        EAscObjectiveConditionPolicy Policy) const;

    bool AreCompletionConditionsImpossible() const;

    void SetState(EAscObjectiveState NewState);
    void RefreshAllConditionSnapshots();
    void UpdateConditionSnapshot(UAscObjectiveConditionComponent* Condition);
    void BroadcastObjectiveStateTransition(EAscObjectiveState OldState, EAscObjectiveState NewState);
    void BroadcastConditionSnapshotDiffs();

    UPROPERTY(Transient)
    TArray<TObjectPtr<UAscObjectiveConditionComponent>> AllConditionComponents;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UAscObjectiveConditionComponent>> CompletionConditions;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UAscObjectiveConditionComponent>> FailureConditions;

    UPROPERTY(ReplicatedUsing = OnRep_State)
    EAscObjectiveState State = EAscObjectiveState::Inactive;

    /** Lightweight replicated state for client UI/BP. Condition components stay server-authoritative. */
    UPROPERTY(ReplicatedUsing = OnRep_ConditionSnapshots)
    TArray<FAscObjectiveConditionSnapshot> ConditionSnapshots;

    UPROPERTY(Transient)
    TArray<FAscObjectiveConditionSnapshot> LastNotifiedConditionSnapshots;

    bool bSuppressConditionEvaluation = false;
};
