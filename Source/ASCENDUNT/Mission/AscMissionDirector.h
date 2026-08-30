#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AscMissionDirector.generated.h"

class AAscMissionDirector;
class AAscObjectiveBase;

UENUM(BlueprintType)
enum class EAscMissionState : uint8
{
    Inactive,
    Running,
    Completed,
    Failed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FAscMissionSimpleEvent,
    AAscMissionDirector*, MissionDirector);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FAscMissionObjectiveChangedEvent,
    AAscMissionDirector*, MissionDirector,
    AAscObjectiveBase*, Objective);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FAscMissionStateChangedEvent,
    AAscMissionDirector*, MissionDirector,
    EAscMissionState, OldState,
    EAscMissionState, NewState);

/**
 * Owns only the ordered objective flow for one mission.
 * Objective-specific gameplay rules stay inside AAscObjectiveBase/conditions.
 */
UCLASS(Blueprintable)
class ASCENDUNT_API AAscMissionDirector : public AActor
{
    GENERATED_BODY()

public:
    AAscMissionDirector();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Mission")
    void StartMission();

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Mission")
    void ResetMission();

    /**
     * Fails the running mission from an external authoritative rule such as a PvE squad wipe.
     * Objective-owned failures still arrive through HandleObjectiveFailed().
     */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Mission")
    void FailMission();

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission")
    AAscObjectiveBase* GetCurrentObjective() const;

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission")
    int32 GetCurrentObjectiveIndex() const { return CurrentObjectiveIndex; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission")
    EAscMissionState GetMissionState() const { return MissionState; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission")
    FText GetMissionTitle() const { return MissionTitle; }

    /** True when the supplied objective is the last configured objective in this mission. */
    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission")
    bool IsFinalObjective(AAscObjectiveBase* Objective) const;

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Mission")
    FAscMissionSimpleEvent OnMissionStarted;

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Mission")
    FAscMissionObjectiveChangedEvent OnObjectiveChanged;

    /** Replicated presentation-safe notification that one objective completed. */
    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Mission")
    FAscMissionObjectiveChangedEvent OnObjectiveCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Mission")
    FAscMissionSimpleEvent OnMissionCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Mission")
    FAscMissionSimpleEvent OnMissionFailed;

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Mission")
    FAscMissionStateChangedEvent OnMissionStateChanged;

protected:
    /** Large player-facing mission announcement shown when the mission starts. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|UI")
    FText MissionTitle;

    /** Ordered objectives placed in the level. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Ascendunt|Mission")
    TArray<TObjectPtr<AAscObjectiveBase>> Objectives;

private:
    UFUNCTION()
    void HandleObjectiveCompleted(AAscObjectiveBase* Objective);

    UFUNCTION()
    void HandleObjectiveFailed(AAscObjectiveBase* Objective);

    UFUNCTION()
    void OnRep_MissionState(EAscMissionState OldState);

    UFUNCTION()
    void OnRep_CurrentObjectiveIndex(int32 OldIndex);

    UFUNCTION()
    void OnRep_LastCompletedObjectiveIndex(int32 OldIndex);

    void BindObjectiveEvents();
    void ActivateCurrentObjective();
    void AdvanceObjective();
    void CompleteMission();
    void SetMissionState(EAscMissionState NewState);
    void BroadcastMissionStateTransition(EAscMissionState OldState, EAscMissionState NewState);

    UPROPERTY(ReplicatedUsing = OnRep_MissionState)
    EAscMissionState MissionState = EAscMissionState::Inactive;

    UPROPERTY(ReplicatedUsing = OnRep_CurrentObjectiveIndex)
    int32 CurrentObjectiveIndex = INDEX_NONE;

    /**
     * Changes once per completed objective so clients receive completion even when
     * CurrentObjectiveIndex advances immediately and the UI already rebound.
     */
    UPROPERTY(ReplicatedUsing = OnRep_LastCompletedObjectiveIndex)
    int32 LastCompletedObjectiveIndex = INDEX_NONE;
};
