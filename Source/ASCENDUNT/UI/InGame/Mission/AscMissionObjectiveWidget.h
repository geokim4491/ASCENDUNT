#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "ASCENDUNT/Mission/AscMissionDirector.h"
#include "ASCENDUNT/Mission/Objective/AscObjectiveTypes.h"
#include "AscMissionObjectiveWidget.generated.h"

class AAscMissionDirector;
class AAscObjectiveBase;
class UProgressBar;
class UTextBlock;
class UWidget;

/**
 * Player-facing mission/objective presentation.
 *
 * C++ decides which presentation event happened, supplies the text/data,
 * and owns the transient title show/collapse lifetime. WBP owns only the animation.
 *
 *  - MissionTitle channel: Mission Start / Success / Fail
 *  - ObjectiveTitle channel: Objective Start (per-objective delayed + timed collapse)
 *  - ObjectiveComplete channel: non-final Objective Complete
 *  - ObjectiveDescription: persistent top-right current objective text/progress
 */
UCLASS()
class ASCENDUNT_API UAscMissionObjectiveWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UAscMissionObjectiveWidget(const FObjectInitializer& ObjectInitializer);

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Mission|UI")
    void BindMissionDirector(AAscMissionDirector* InMissionDirector);

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Mission|UI")
    void UnbindMissionDirector();

    /** Demo convenience: binds the first MissionDirector found in the current world. */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Mission|UI")
    bool TryAutoBindMissionDirector();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UFUNCTION()
    void HandleObjectiveChanged(AAscMissionDirector* MissionDirector, AAscObjectiveBase* Objective);

    /** Replicated by MissionDirector so clients do not miss completion during immediate objective advance. */
    UFUNCTION()
    void HandleObjectiveCompleted(AAscMissionDirector* MissionDirector, AAscObjectiveBase* Objective);

    UFUNCTION()
    void HandleMissionStateChanged(
        AAscMissionDirector* MissionDirector,
        EAscMissionState OldState,
        EAscMissionState NewState);

    UFUNCTION()
    void HandleObjectiveStateChanged(
        AAscObjectiveBase* Objective,
        EAscObjectiveState OldState,
        EAscObjectiveState NewState);

    UFUNCTION()
    void HandleConditionProgressChanged(
        AAscObjectiveBase* Objective,
        FGameplayTag ConditionId,
        float CurrentValue,
        float TargetValue);

    /** Optional wrapper for the whole mission UI. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Mission|UI")
    TObjectPtr<UWidget> MissionObjectiveRoot;

    /** Center announcement wrapper. C++ owns show/collapse timing; WBP owns the animation itself. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Mission|UI|Mission Title")
    TObjectPtr<UWidget> MissionTitleRoot;

    /** Center text used by Mission Start/Success/Fail. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Mission|UI|Mission Title")
    TObjectPtr<UTextBlock> Txt_MissionTitle;

    /** Separate Objective Complete presentation. Final objective completion skips this channel. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Mission|UI|Objective Complete")
    TObjectPtr<UWidget> ObjectiveCompleteRoot;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Mission|UI|Objective Complete")
    TObjectPtr<UTextBlock> Txt_ObjectiveComplete;

    /** Top-center objective-title wrapper. C++ owns show/collapse timing; WBP owns the animation itself. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Mission|UI|Objective Title")
    TObjectPtr<UWidget> ObjectiveTitleRoot;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Mission|UI|Objective Title")
    TObjectPtr<UTextBlock> Txt_ObjectiveTitle;

    /** Persistent top-right objective-description wrapper. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Mission|UI|Objective Description")
    TObjectPtr<UWidget> ObjectiveDescriptionRoot;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Mission|UI|Objective Description")
    TObjectPtr<UTextBlock> Txt_ObjectiveDescription;

    /** Usually placed under ObjectiveDescriptionRoot in the top-right panel. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Mission|UI|Progress")
    TObjectPtr<UWidget> ObjectiveProgressRoot;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Mission|UI|Progress")
    TObjectPtr<UTextBlock> Txt_ObjectiveProgressLabel;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Mission|UI|Progress")
    TObjectPtr<UTextBlock> Txt_ObjectiveProgressValue;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Mission|UI|Progress")
    TObjectPtr<UProgressBar> Progress_Objective;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Mission|UI|Objective Complete")
    FText ObjectiveCompleteMessage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Mission|UI|Mission Title")
    FText MissionCompleteMessage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Mission|UI|Mission Title")
    FText MissionFailedMessage;

    /** C++ owns each transient channel's visible lifetime and collapses it when its duration ends. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Mission|UI|Timing", meta = (ClampMin = "0.01"))
    float MissionStartTitleDuration = 2.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Mission|UI|Timing", meta = (ClampMin = "0.01"))
    float ObjectiveCompleteDuration = 1.75f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Mission|UI|Timing", meta = (ClampMin = "0.01"))
    float MissionResultDuration = 3.0f;

    /**
     * Delay from the mission entering Running until ObjectiveDescriptionRoot is first shown.
     * Match this to the Mission Start presentation lifetime when the description should appear as it ends.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Mission|UI|Timing", meta = (ClampMin = "0.0"))
    float ObjectiveDescriptionInitialDelay = 3.0f;

    /**
     * Delay from ObjectiveTitleRoot being presented until the persistent description/progress
     * switches to the newly active Objective. Match this to the Objective Start animation lifetime
     * when the top-right panel should update as that presentation disappears.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Mission|UI|Timing", meta = (ClampMin = "0.0"))
    float ObjectiveDescriptionUpdateDelay = 3.5f;

    /** Mission begins. MissionTitleRoot is already visible when this event fires. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Ascendunt|Mission|UI|Animation")
    void BP_OnMissionTitleStart(const FText& Message);

    /** Mission completes successfully. MissionTitleRoot is already visible when this event fires. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Ascendunt|Mission|UI|Animation")
    void BP_OnMissionTitleSuccess(const FText& Message);


    /** Mission fails. MissionTitleRoot is already visible when this event fires. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Ascendunt|Mission|UI|Animation")
    void BP_OnMissionTitleFail(const FText& Message);

    /** Current Objective becomes active after ObjectiveTitleDisplayDelay. ObjectiveTitleRoot is already visible. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Ascendunt|Mission|UI|Animation")
    void BP_OnObjectiveStart(const FText& Message);

    /** A non-final Objective completes. ObjectiveCompleteRoot is already visible when this event fires. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Ascendunt|Mission|UI|Animation")
    void BP_OnObjectiveComplete(const FText& Message);

private:
    enum class EQueuedMissionAnnouncementType : uint8
    {
        MissionStart,
        MissionSuccess,
        MissionFail
    };

    struct FQueuedMissionAnnouncement
    {
        EQueuedMissionAnnouncementType Type = EQueuedMissionAnnouncementType::MissionStart;
        FText Message;
        float DurationSeconds = 0.0f;
    };

    void BindObjective(AAscObjectiveBase* NewObjective);
    void UnbindObjective();

    void RefreshPersistentObjectiveUI();
    void RefreshObjectiveDescription();
    void RefreshProgress();
    void SetProgressVisibility(bool bVisible);

    void ScheduleInitialObjectiveDescription();
    void ScheduleObjectiveDescriptionUpdate();
    void ApplyBoundObjectiveToPersistentUI();
    void ClearObjectiveDescriptionTimers();
    void ClearObjectiveDescriptionPresentation();

    void QueueMissionAnnouncement(
        EQueuedMissionAnnouncementType Type,
        const FText& Message,
        float DurationSeconds);
    void TryPresentNextMissionAnnouncement();
    void FinishCurrentMissionAnnouncement();
    void ClearMissionAnnouncements();
    void SetMissionTitleVisibility(bool bVisible);

    void PresentObjectiveComplete();
    void FinishObjectiveCompletePresentation();
    void ClearObjectiveCompletePresentation();
    void SetObjectiveCompleteVisibility(bool bVisible);

    void ScheduleObjectiveTitle();
    void PresentObjectiveTitle();
    void FinishObjectiveTitlePresentation();
    void ClearObjectiveTitlePresentation();
    void SetObjectiveTitleVisibility(bool bVisible);

    void SetDescriptionVisibility(bool bVisible);

    bool ResolveDisplayedSnapshot(FAscObjectiveConditionSnapshot& OutSnapshot) const;
    EAscObjectiveProgressDisplayType ResolveDisplayType(const FGameplayTag& ConditionId) const;
    FText ResolveProgressLabel(const FGameplayTag& ConditionId) const;

    TWeakObjectPtr<AAscMissionDirector> BoundMissionDirector;
    TWeakObjectPtr<AAscObjectiveBase> BoundObjective;
    TWeakObjectPtr<AAscObjectiveBase> DisplayedDescriptionObjective;

    TArray<FQueuedMissionAnnouncement> MissionAnnouncementQueue;
    bool bMissionAnnouncementActive = false;
    bool bObjectiveTitleScheduledOrShown = false;
    bool bMissionStartAnnouncementPresented = false;
    bool bObjectiveDescriptionHasBeenPresented = false;

    FTimerHandle MissionAnnouncementTimerHandle;
    FTimerHandle ObjectiveCompleteTimerHandle;
    FTimerHandle ObjectiveTitleDelayTimerHandle;
    FTimerHandle ObjectiveTitleDisplayTimerHandle;
    FTimerHandle ObjectiveDescriptionInitialDelayTimerHandle;
    FTimerHandle ObjectiveDescriptionUpdateDelayTimerHandle;
};
