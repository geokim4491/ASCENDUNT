#include "ASCENDUNT/UI/InGame/Mission/AscMissionObjectiveWidget.h"

#include "ASCENDUNT/Mission/Conditions/AscObjectiveConditionComponent.h"
#include "ASCENDUNT/Mission/Objective/AscObjectiveBase.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "EngineUtils.h"
#include "Engine/World.h"

UAscMissionObjectiveWidget::UAscMissionObjectiveWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    ObjectiveCompleteMessage = FText::FromString(TEXT("목표 완료"));
    MissionCompleteMessage = FText::FromString(TEXT("미션 성공"));
    MissionFailedMessage = FText::FromString(TEXT("미션 실패"));
}

void UAscMissionObjectiveWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Transient announcement channels start collapsed. C++ owns their visibility lifetime.
    SetMissionTitleVisibility(false);
    SetObjectiveCompleteVisibility(false);
    SetObjectiveTitleVisibility(false);
    SetDescriptionVisibility(false);
    SetProgressVisibility(false);

    if (MissionObjectiveRoot)
    {
        MissionObjectiveRoot->SetVisibility(ESlateVisibility::HitTestInvisible);
    }

    TryAutoBindMissionDirector();
}

void UAscMissionObjectiveWidget::NativeDestruct()
{
    UnbindMissionDirector();
    Super::NativeDestruct();
}

void UAscMissionObjectiveWidget::BindMissionDirector(AAscMissionDirector* InMissionDirector)
{
    if (BoundMissionDirector.Get() == InMissionDirector)
    {
        if (InMissionDirector
            && InMissionDirector->GetMissionState() == EAscMissionState::Running
            && !bObjectiveDescriptionHasBeenPresented)
        {
            ScheduleInitialObjectiveDescription();
        }

        RefreshPersistentObjectiveUI();
        return;
    }

    UnbindMissionDirector();

    if (!IsValid(InMissionDirector))
    {
        return;
    }

    BoundMissionDirector = InMissionDirector;
    bMissionStartAnnouncementPresented = false;
    bObjectiveDescriptionHasBeenPresented = false;
    DisplayedDescriptionObjective.Reset();

    InMissionDirector->OnObjectiveChanged.AddDynamic(
        this,
        &ThisClass::HandleObjectiveChanged);
    InMissionDirector->OnObjectiveCompleted.AddDynamic(
        this,
        &ThisClass::HandleObjectiveCompleted);
    InMissionDirector->OnMissionStateChanged.AddDynamic(
        this,
        &ThisClass::HandleMissionStateChanged);

    if (MissionObjectiveRoot)
    {
        MissionObjectiveRoot->SetVisibility(ESlateVisibility::HitTestInvisible);
    }

    BindObjective(InMissionDirector->GetCurrentObjective());
    RefreshPersistentObjectiveUI();

    // Handles HUD/widget construction after MissionState already replicated to Running.
    if (InMissionDirector->GetMissionState() == EAscMissionState::Running)
    {
        const FText MissionTitle = InMissionDirector->GetMissionTitle();
        if (!MissionTitle.IsEmpty())
        {
            QueueMissionAnnouncement(
                EQueuedMissionAnnouncementType::MissionStart,
                MissionTitle,
                MissionStartTitleDuration);
            bMissionStartAnnouncementPresented = true;
        }

        ScheduleInitialObjectiveDescription();
    }
}

void UAscMissionObjectiveWidget::UnbindMissionDirector()
{
    UnbindObjective();
    ClearMissionAnnouncements();
    ClearObjectiveCompletePresentation();
    ClearObjectiveTitlePresentation();
    ClearObjectiveDescriptionPresentation();

    if (AAscMissionDirector* MissionDirector = BoundMissionDirector.Get())
    {
        MissionDirector->OnObjectiveChanged.RemoveDynamic(
            this,
            &ThisClass::HandleObjectiveChanged);
        MissionDirector->OnObjectiveCompleted.RemoveDynamic(
            this,
            &ThisClass::HandleObjectiveCompleted);
        MissionDirector->OnMissionStateChanged.RemoveDynamic(
            this,
            &ThisClass::HandleMissionStateChanged);
    }

    BoundMissionDirector.Reset();
    bMissionStartAnnouncementPresented = false;

    if (MissionObjectiveRoot)
    {
        MissionObjectiveRoot->SetVisibility(ESlateVisibility::Collapsed);
    }
}

bool UAscMissionObjectiveWidget::TryAutoBindMissionDirector()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    AAscMissionDirector* FirstMissionDirector = nullptr;
    int32 NumMissionDirectors = 0;

    for (TActorIterator<AAscMissionDirector> It(World); It; ++It)
    {
        if (!IsValid(*It))
        {
            continue;
        }

        ++NumMissionDirectors;
        if (!FirstMissionDirector)
        {
            FirstMissionDirector = *It;
        }
    }

    if (!FirstMissionDirector)
    {
        return false;
    }

    if (NumMissionDirectors > 1)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[MissionUI] Found %d MissionDirectors. Auto-bind uses '%s'. Bind explicitly when multiple missions coexist."),
            NumMissionDirectors,
            *FirstMissionDirector->GetName());
    }

    BindMissionDirector(FirstMissionDirector);
    return true;
}

void UAscMissionObjectiveWidget::HandleObjectiveChanged(
    AAscMissionDirector* MissionDirector,
    AAscObjectiveBase* Objective)
{
    if (MissionDirector != BoundMissionDirector.Get())
    {
        return;
    }

    // Keep the previous top-right description visible while the new ObjectiveTitle presentation plays.
    // Any pending update for an older objective is invalid once the current objective changes.
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ObjectiveDescriptionUpdateDelayTimerHandle);
    }

    BindObjective(Objective);

    // If this objective has no title presentation, still allow the persistent panel to advance.
    if (bObjectiveDescriptionHasBeenPresented
        && Objective
        && Objective->GetObjectiveTitle().IsEmpty())
    {
        ScheduleObjectiveDescriptionUpdate();
    }
}

void UAscMissionObjectiveWidget::HandleObjectiveCompleted(
    AAscMissionDirector* MissionDirector,
    AAscObjectiveBase* Objective)
{
    if (MissionDirector != BoundMissionDirector.Get())
    {
        return;
    }

    // The final objective promotes directly to Mission Success. Do not show
    // an intermediate "Objective Complete" announcement for it.
    if (MissionDirector->IsFinalObjective(Objective))
    {
        ClearObjectiveCompletePresentation();
        return;
    }

    PresentObjectiveComplete();
}

void UAscMissionObjectiveWidget::HandleMissionStateChanged(
    AAscMissionDirector* MissionDirector,
    EAscMissionState OldState,
    EAscMissionState NewState)
{
    if (MissionDirector != BoundMissionDirector.Get())
    {
        return;
    }

    switch (NewState)
    {
    case EAscMissionState::Running:
        if (!bMissionStartAnnouncementPresented)
        {
            const FText MissionTitle = MissionDirector->GetMissionTitle();
            if (!MissionTitle.IsEmpty())
            {
                QueueMissionAnnouncement(
                    EQueuedMissionAnnouncementType::MissionStart,
                    MissionTitle,
                    MissionStartTitleDuration);
            }
            bMissionStartAnnouncementPresented = true;
        }

        ScheduleInitialObjectiveDescription();
        break;

    case EAscMissionState::Completed:
        ClearObjectiveCompletePresentation();
        ClearObjectiveTitlePresentation();
        ClearObjectiveDescriptionPresentation();
        if (!MissionCompleteMessage.IsEmpty())
        {
            QueueMissionAnnouncement(
                EQueuedMissionAnnouncementType::MissionSuccess,
                MissionCompleteMessage,
                MissionResultDuration);
        }
        break;

    case EAscMissionState::Failed:
        ClearObjectiveCompletePresentation();
        ClearObjectiveTitlePresentation();
        ClearObjectiveDescriptionPresentation();
        if (!MissionFailedMessage.IsEmpty())
        {
            QueueMissionAnnouncement(
                EQueuedMissionAnnouncementType::MissionFail,
                MissionFailedMessage,
                MissionResultDuration);
        }
        break;

    case EAscMissionState::Inactive:
    default:
        ClearMissionAnnouncements();
        ClearObjectiveCompletePresentation();
        ClearObjectiveTitlePresentation();
        ClearObjectiveDescriptionPresentation();
        bMissionStartAnnouncementPresented = false;
        break;
    }

    RefreshPersistentObjectiveUI();
}

void UAscMissionObjectiveWidget::HandleObjectiveStateChanged(
    AAscObjectiveBase* Objective,
    EAscObjectiveState OldState,
    EAscObjectiveState NewState)
{
    if (Objective != BoundObjective.Get())
    {
        return;
    }

    if (NewState == EAscObjectiveState::Active)
    {
        ScheduleObjectiveTitle();
    }
    else if (NewState == EAscObjectiveState::Completed
        || NewState == EAscObjectiveState::Failed
        || NewState == EAscObjectiveState::Inactive)
    {
        // Only cancel pending Objective Start presentation. WBP owns visual cleanup.
        ClearObjectiveTitlePresentation();
    }

    RefreshPersistentObjectiveUI();
}

void UAscMissionObjectiveWidget::HandleConditionProgressChanged(
    AAscObjectiveBase* Objective,
    FGameplayTag ConditionId,
    float CurrentValue,
    float TargetValue)
{
    if (Objective != BoundObjective.Get())
    {
        return;
    }

    // Do not update the top-right panel with the new objective until its configured
    // description-update delay has elapsed.
    if (Objective != DisplayedDescriptionObjective.Get())
    {
        return;
    }

    const FGameplayTag ExplicitProgressCondition = Objective->GetProgressConditionId();
    if (ExplicitProgressCondition.IsValid() && ExplicitProgressCondition != ConditionId)
    {
        return;
    }

    RefreshProgress();
}

void UAscMissionObjectiveWidget::BindObjective(AAscObjectiveBase* NewObjective)
{
    if (BoundObjective.Get() == NewObjective)
    {
        if (NewObjective && NewObjective->IsActive() && !bObjectiveTitleScheduledOrShown)
        {
            ScheduleObjectiveTitle();
        }
        return;
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ObjectiveDescriptionUpdateDelayTimerHandle);
    }

    UnbindObjective();
    ClearObjectiveTitlePresentation();

    if (!IsValid(NewObjective))
    {
        return;
    }

    BoundObjective = NewObjective;
    NewObjective->OnObjectiveStateChanged.AddDynamic(
        this,
        &ThisClass::HandleObjectiveStateChanged);
    NewObjective->OnConditionProgressChanged.AddDynamic(
        this,
        &ThisClass::HandleConditionProgressChanged);

    // Replication order is not guaranteed. If State arrived before CurrentObjectiveIndex,
    // schedule the title here instead of waiting for an Active transition that already happened.
    if (NewObjective->IsActive())
    {
        ScheduleObjectiveTitle();
    }
}

void UAscMissionObjectiveWidget::UnbindObjective()
{
    if (AAscObjectiveBase* Objective = BoundObjective.Get())
    {
        Objective->OnObjectiveStateChanged.RemoveDynamic(
            this,
            &ThisClass::HandleObjectiveStateChanged);
        Objective->OnConditionProgressChanged.RemoveDynamic(
            this,
            &ThisClass::HandleConditionProgressChanged);
    }

    BoundObjective.Reset();
    ClearObjectiveTitlePresentation();
}

void UAscMissionObjectiveWidget::RefreshPersistentObjectiveUI()
{
    const AAscMissionDirector* MissionDirector = BoundMissionDirector.Get();
    const AAscObjectiveBase* Objective = BoundObjective.Get();
    const bool bMissionRunning = MissionDirector
        && MissionDirector->GetMissionState() == EAscMissionState::Running;

    if (!bMissionRunning || !Objective)
    {
        SetDescriptionVisibility(false);
        SetProgressVisibility(false);
        return;
    }

    // During an objective transition, keep displaying the old persistent data until
    // the new ObjectiveTitle presentation has had time to finish.
    if (DisplayedDescriptionObjective.Get() != Objective)
    {
        return;
    }

    RefreshObjectiveDescription();
    RefreshProgress();
}

void UAscMissionObjectiveWidget::ScheduleInitialObjectiveDescription()
{
    if (bObjectiveDescriptionHasBeenPresented)
    {
        return;
    }

    AAscMissionDirector* MissionDirector = BoundMissionDirector.Get();
    if (!MissionDirector || MissionDirector->GetMissionState() != EAscMissionState::Running)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    World->GetTimerManager().ClearTimer(ObjectiveDescriptionInitialDelayTimerHandle);

    const float Delay = FMath::Max(0.0f, ObjectiveDescriptionInitialDelay);
    if (Delay <= KINDA_SMALL_NUMBER)
    {
        ApplyBoundObjectiveToPersistentUI();
        return;
    }

    World->GetTimerManager().SetTimer(
        ObjectiveDescriptionInitialDelayTimerHandle,
        this,
        &ThisClass::ApplyBoundObjectiveToPersistentUI,
        Delay,
        false);
}

void UAscMissionObjectiveWidget::ScheduleObjectiveDescriptionUpdate()
{
    if (!bObjectiveDescriptionHasBeenPresented)
    {
        // The very first appearance is governed only by ObjectiveDescriptionInitialDelay.
        return;
    }

    AAscObjectiveBase* Objective = BoundObjective.Get();
    if (!Objective || DisplayedDescriptionObjective.Get() == Objective)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    World->GetTimerManager().ClearTimer(ObjectiveDescriptionUpdateDelayTimerHandle);

    const float Delay = FMath::Max(0.0f, ObjectiveDescriptionUpdateDelay);
    if (Delay <= KINDA_SMALL_NUMBER)
    {
        ApplyBoundObjectiveToPersistentUI();
        return;
    }

    World->GetTimerManager().SetTimer(
        ObjectiveDescriptionUpdateDelayTimerHandle,
        this,
        &ThisClass::ApplyBoundObjectiveToPersistentUI,
        Delay,
        false);
}

void UAscMissionObjectiveWidget::ApplyBoundObjectiveToPersistentUI()
{
    UWorld* World = GetWorld();
    if (World)
    {
        World->GetTimerManager().ClearTimer(ObjectiveDescriptionInitialDelayTimerHandle);
        World->GetTimerManager().ClearTimer(ObjectiveDescriptionUpdateDelayTimerHandle);
    }

    AAscMissionDirector* MissionDirector = BoundMissionDirector.Get();
    AAscObjectiveBase* Objective = BoundObjective.Get();
    if (!MissionDirector
        || MissionDirector->GetMissionState() != EAscMissionState::Running
        || !Objective)
    {
        return;
    }

    DisplayedDescriptionObjective = Objective;
    bObjectiveDescriptionHasBeenPresented = true;

    RefreshObjectiveDescription();
    RefreshProgress();
}

void UAscMissionObjectiveWidget::ClearObjectiveDescriptionTimers()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ObjectiveDescriptionInitialDelayTimerHandle);
        World->GetTimerManager().ClearTimer(ObjectiveDescriptionUpdateDelayTimerHandle);
    }
}

void UAscMissionObjectiveWidget::ClearObjectiveDescriptionPresentation()
{
    ClearObjectiveDescriptionTimers();

    DisplayedDescriptionObjective.Reset();
    bObjectiveDescriptionHasBeenPresented = false;

    if (Txt_ObjectiveDescription)
    {
        Txt_ObjectiveDescription->SetText(FText::GetEmpty());
    }

    SetDescriptionVisibility(false);
    SetProgressVisibility(false);
}

void UAscMissionObjectiveWidget::RefreshObjectiveDescription()
{
    const AAscObjectiveBase* Objective = BoundObjective.Get();
    const FText Description = Objective
        ? Objective->GetObjectiveDescription()
        : FText::GetEmpty();

    if (Txt_ObjectiveDescription)
    {
        Txt_ObjectiveDescription->SetText(Description);
    }

    SetDescriptionVisibility(Objective && !Description.IsEmpty());
}

void UAscMissionObjectiveWidget::RefreshProgress()
{
    const AAscMissionDirector* MissionDirector = BoundMissionDirector.Get();
    if (!MissionDirector || MissionDirector->GetMissionState() != EAscMissionState::Running)
    {
        SetProgressVisibility(false);
        return;
    }

    FAscObjectiveConditionSnapshot Snapshot;
    if (!ResolveDisplayedSnapshot(Snapshot))
    {
        SetProgressVisibility(false);
        return;
    }

    const EAscObjectiveProgressDisplayType DisplayType = ResolveDisplayType(Snapshot.ConditionId);
    if (DisplayType == EAscObjectiveProgressDisplayType::Hidden)
    {
        SetProgressVisibility(false);
        return;
    }

    SetProgressVisibility(true);

    const FText ProgressLabel = ResolveProgressLabel(Snapshot.ConditionId);
    if (Txt_ObjectiveProgressLabel)
    {
        Txt_ObjectiveProgressLabel->SetText(ProgressLabel);
        Txt_ObjectiveProgressLabel->SetVisibility(
            ProgressLabel.IsEmpty()
                ? ESlateVisibility::Collapsed
                : ESlateVisibility::HitTestInvisible);
    }

    const float SafeTarget = FMath::Max(Snapshot.TargetValue, KINDA_SMALL_NUMBER);
    const float ProgressRatio = FMath::Clamp(Snapshot.CurrentValue / SafeTarget, 0.0f, 1.0f);

    if (Progress_Objective)
    {
        Progress_Objective->SetPercent(ProgressRatio);
    }

    if (!Txt_ObjectiveProgressValue)
    {
        return;
    }

    if (DisplayType == EAscObjectiveProgressDisplayType::Count)
    {
        Txt_ObjectiveProgressValue->SetText(FText::Format(
            FText::FromString(TEXT("{0} / {1}")),
            FText::AsNumber(FMath::RoundToInt(Snapshot.CurrentValue)),
            FText::AsNumber(FMath::RoundToInt(Snapshot.TargetValue))));
    }
    else
    {
        Txt_ObjectiveProgressValue->SetText(FText::AsPercent(ProgressRatio));
    }
}

void UAscMissionObjectiveWidget::SetProgressVisibility(bool bVisible)
{
    const ESlateVisibility DesiredVisibility = bVisible
        ? ESlateVisibility::HitTestInvisible
        : ESlateVisibility::Collapsed;

    if (ObjectiveProgressRoot)
    {
        ObjectiveProgressRoot->SetVisibility(DesiredVisibility);
        return;
    }

    if (Progress_Objective)
    {
        Progress_Objective->SetVisibility(DesiredVisibility);
    }
    if (Txt_ObjectiveProgressLabel)
    {
        Txt_ObjectiveProgressLabel->SetVisibility(DesiredVisibility);
    }
    if (Txt_ObjectiveProgressValue)
    {
        Txt_ObjectiveProgressValue->SetVisibility(DesiredVisibility);
    }
}

void UAscMissionObjectiveWidget::QueueMissionAnnouncement(
    EQueuedMissionAnnouncementType Type,
    const FText& Message,
    float DurationSeconds)
{
    if (Message.IsEmpty())
    {
        return;
    }

    FQueuedMissionAnnouncement& Entry = MissionAnnouncementQueue.AddDefaulted_GetRef();
    Entry.Type = Type;
    Entry.Message = Message;
    Entry.DurationSeconds = FMath::Max(DurationSeconds, 0.01f);

    TryPresentNextMissionAnnouncement();
}

void UAscMissionObjectiveWidget::TryPresentNextMissionAnnouncement()
{
    if (bMissionAnnouncementActive || MissionAnnouncementQueue.IsEmpty())
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const FQueuedMissionAnnouncement Entry = MissionAnnouncementQueue[0];
    MissionAnnouncementQueue.RemoveAt(0);

    bMissionAnnouncementActive = true;

    if (Txt_MissionTitle)
    {
        Txt_MissionTitle->SetText(Entry.Message);
    }

    SetMissionTitleVisibility(true);

    switch (Entry.Type)
    {
    case EQueuedMissionAnnouncementType::MissionStart:
        BP_OnMissionTitleStart(Entry.Message);
        break;

    case EQueuedMissionAnnouncementType::MissionSuccess:
        BP_OnMissionTitleSuccess(Entry.Message);
        break;

    case EQueuedMissionAnnouncementType::MissionFail:
        BP_OnMissionTitleFail(Entry.Message);
        break;

    default:
        break;
    }

    World->GetTimerManager().SetTimer(
        MissionAnnouncementTimerHandle,
        this,
        &ThisClass::FinishCurrentMissionAnnouncement,
        Entry.DurationSeconds,
        false);
}

void UAscMissionObjectiveWidget::FinishCurrentMissionAnnouncement()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(MissionAnnouncementTimerHandle);
    }

    bMissionAnnouncementActive = false;
    SetMissionTitleVisibility(false);

    TryPresentNextMissionAnnouncement();
}

void UAscMissionObjectiveWidget::ClearMissionAnnouncements()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(MissionAnnouncementTimerHandle);
    }

    MissionAnnouncementQueue.Reset();
    bMissionAnnouncementActive = false;

    if (Txt_MissionTitle)
    {
        Txt_MissionTitle->SetText(FText::GetEmpty());
    }
    SetMissionTitleVisibility(false);
}

void UAscMissionObjectiveWidget::SetMissionTitleVisibility(bool bVisible)
{
    const ESlateVisibility DesiredVisibility = bVisible
        ? ESlateVisibility::HitTestInvisible
        : ESlateVisibility::Collapsed;

    if (MissionTitleRoot)
    {
        MissionTitleRoot->SetVisibility(DesiredVisibility);
    }
    else if (Txt_MissionTitle)
    {
        Txt_MissionTitle->SetVisibility(DesiredVisibility);
    }
}

void UAscMissionObjectiveWidget::PresentObjectiveComplete()
{
    if (ObjectiveCompleteMessage.IsEmpty())
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    World->GetTimerManager().ClearTimer(ObjectiveCompleteTimerHandle);

    if (Txt_ObjectiveComplete)
    {
        Txt_ObjectiveComplete->SetText(ObjectiveCompleteMessage);
    }

    SetObjectiveCompleteVisibility(true);
    BP_OnObjectiveComplete(ObjectiveCompleteMessage);

    World->GetTimerManager().SetTimer(
        ObjectiveCompleteTimerHandle,
        this,
        &ThisClass::FinishObjectiveCompletePresentation,
        FMath::Max(ObjectiveCompleteDuration, 0.01f),
        false);
}

void UAscMissionObjectiveWidget::FinishObjectiveCompletePresentation()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ObjectiveCompleteTimerHandle);
    }

    SetObjectiveCompleteVisibility(false);
}

void UAscMissionObjectiveWidget::ClearObjectiveCompletePresentation()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ObjectiveCompleteTimerHandle);
    }

    if (Txt_ObjectiveComplete)
    {
        Txt_ObjectiveComplete->SetText(FText::GetEmpty());
    }

    SetObjectiveCompleteVisibility(false);
}

void UAscMissionObjectiveWidget::SetObjectiveCompleteVisibility(bool bVisible)
{
    const ESlateVisibility DesiredVisibility = bVisible
        ? ESlateVisibility::HitTestInvisible
        : ESlateVisibility::Collapsed;

    if (ObjectiveCompleteRoot)
    {
        ObjectiveCompleteRoot->SetVisibility(DesiredVisibility);
    }
    else if (Txt_ObjectiveComplete)
    {
        Txt_ObjectiveComplete->SetVisibility(DesiredVisibility);
    }
}

void UAscMissionObjectiveWidget::ScheduleObjectiveTitle()
{
    AAscObjectiveBase* Objective = BoundObjective.Get();
    if (!Objective || !Objective->IsActive() || bObjectiveTitleScheduledOrShown)
    {
        return;
    }

    const FText ObjectiveTitle = Objective->GetObjectiveTitle();
    if (ObjectiveTitle.IsEmpty())
    {
        return;
    }

    bObjectiveTitleScheduledOrShown = true;

    const float Delay = FMath::Max(0.0f, Objective->GetObjectiveTitleDisplayDelay());
    if (Delay <= KINDA_SMALL_NUMBER)
    {
        PresentObjectiveTitle();
        return;
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            ObjectiveTitleDelayTimerHandle,
            this,
            &ThisClass::PresentObjectiveTitle,
            Delay,
            false);
    }
}

void UAscMissionObjectiveWidget::PresentObjectiveTitle()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ObjectiveTitleDelayTimerHandle);
    }

    AAscObjectiveBase* Objective = BoundObjective.Get();
    if (!Objective || !Objective->IsActive())
    {
        ClearObjectiveTitlePresentation();
        return;
    }

    const FText ObjectiveTitle = Objective->GetObjectiveTitle();
    if (ObjectiveTitle.IsEmpty())
    {
        ClearObjectiveTitlePresentation();
        return;
    }

    if (Txt_ObjectiveTitle)
    {
        Txt_ObjectiveTitle->SetText(ObjectiveTitle);
    }

    SetObjectiveTitleVisibility(true);
    BP_OnObjectiveStart(ObjectiveTitle);

    // For subsequent objectives, start the persistent-panel update countdown when
    // the new ObjectiveTitle presentation actually begins.
    ScheduleObjectiveDescriptionUpdate();

    const float Duration = Objective->GetObjectiveTitleDisplayDuration();
    if (Duration > 0.0f)
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(
                ObjectiveTitleDisplayTimerHandle,
                this,
                &ThisClass::FinishObjectiveTitlePresentation,
                Duration,
                false);
        }
    }
}

void UAscMissionObjectiveWidget::FinishObjectiveTitlePresentation()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ObjectiveTitleDisplayTimerHandle);
    }

    SetObjectiveTitleVisibility(false);
}

void UAscMissionObjectiveWidget::ClearObjectiveTitlePresentation()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ObjectiveTitleDelayTimerHandle);
        World->GetTimerManager().ClearTimer(ObjectiveTitleDisplayTimerHandle);
        World->GetTimerManager().ClearTimer(ObjectiveDescriptionUpdateDelayTimerHandle);
    }

    bObjectiveTitleScheduledOrShown = false;

    if (Txt_ObjectiveTitle)
    {
        Txt_ObjectiveTitle->SetText(FText::GetEmpty());
    }
    SetObjectiveTitleVisibility(false);
}

void UAscMissionObjectiveWidget::SetObjectiveTitleVisibility(bool bVisible)
{
    const ESlateVisibility DesiredVisibility = bVisible
        ? ESlateVisibility::HitTestInvisible
        : ESlateVisibility::Collapsed;

    if (ObjectiveTitleRoot)
    {
        ObjectiveTitleRoot->SetVisibility(DesiredVisibility);
    }
    else if (Txt_ObjectiveTitle)
    {
        Txt_ObjectiveTitle->SetVisibility(DesiredVisibility);
    }
}

void UAscMissionObjectiveWidget::SetDescriptionVisibility(bool bVisible)
{
    const ESlateVisibility DesiredVisibility = bVisible
        ? ESlateVisibility::HitTestInvisible
        : ESlateVisibility::Collapsed;

    if (ObjectiveDescriptionRoot)
    {
        ObjectiveDescriptionRoot->SetVisibility(DesiredVisibility);
    }
    else if (Txt_ObjectiveDescription)
    {
        Txt_ObjectiveDescription->SetVisibility(DesiredVisibility);
    }
}

bool UAscMissionObjectiveWidget::ResolveDisplayedSnapshot(FAscObjectiveConditionSnapshot& OutSnapshot) const
{
    const AAscObjectiveBase* Objective = BoundObjective.Get();
    if (!Objective)
    {
        return false;
    }

    const TArray<FAscObjectiveConditionSnapshot> Snapshots = Objective->GetConditionSnapshots();
    if (Snapshots.IsEmpty())
    {
        return false;
    }

    const FGameplayTag ExplicitConditionId = Objective->GetProgressConditionId();
    if (ExplicitConditionId.IsValid())
    {
        for (const FAscObjectiveConditionSnapshot& Snapshot : Snapshots)
        {
            if (Snapshot.ConditionId == ExplicitConditionId
                && ResolveDisplayType(Snapshot.ConditionId) != EAscObjectiveProgressDisplayType::Hidden)
            {
                OutSnapshot = Snapshot;
                return true;
            }
        }

        return false;
    }

    for (const FAscObjectiveConditionSnapshot& Snapshot : Snapshots)
    {
        if (ResolveDisplayType(Snapshot.ConditionId) != EAscObjectiveProgressDisplayType::Hidden)
        {
            OutSnapshot = Snapshot;
            return true;
        }
    }

    return false;
}

EAscObjectiveProgressDisplayType UAscMissionObjectiveWidget::ResolveDisplayType(
    const FGameplayTag& ConditionId) const
{
    if (const AAscObjectiveBase* Objective = BoundObjective.Get())
    {
        if (const UAscObjectiveConditionComponent* Condition = Objective->GetConditionComponent(ConditionId))
        {
            return Condition->GetProgressDisplayType();
        }
    }

    return EAscObjectiveProgressDisplayType::Hidden;
}

FText UAscMissionObjectiveWidget::ResolveProgressLabel(const FGameplayTag& ConditionId) const
{
    if (const AAscObjectiveBase* Objective = BoundObjective.Get())
    {
        if (const UAscObjectiveConditionComponent* Condition = Objective->GetConditionComponent(ConditionId))
        {
            return Condition->GetProgressLabel();
        }
    }

    return FText::GetEmpty();
}
