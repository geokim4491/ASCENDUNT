#include "ASCENDUNT/Mission/AscMissionDirector.h"

#include "ASCENDUNT/Mission/Objective/AscObjectiveBase.h"
#include "Net/UnrealNetwork.h"

AAscMissionDirector::AAscMissionDirector()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(false);
}

void AAscMissionDirector::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AAscMissionDirector, MissionState);
    DOREPLIFETIME(AAscMissionDirector, CurrentObjectiveIndex);
    DOREPLIFETIME(AAscMissionDirector, LastCompletedObjectiveIndex);
}

void AAscMissionDirector::StartMission()
{
    if (!HasAuthority() || MissionState != EAscMissionState::Inactive)
    {
        return;
    }

    BindObjectiveEvents();

    if (Objectives.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Mission] '%s' has no objectives."), *GetName());
        CompleteMission();
        return;
    }

    CurrentObjectiveIndex = 0;
    SetMissionState(EAscMissionState::Running);
    OnObjectiveChanged.Broadcast(this, GetCurrentObjective());
    ForceNetUpdate();

    ActivateCurrentObjective();
}

void AAscMissionDirector::ResetMission()
{
    if (!HasAuthority())
    {
        return;
    }

    for (AAscObjectiveBase* Objective : Objectives)
    {
        if (Objective)
        {
            Objective->ResetObjective();
        }
    }

    CurrentObjectiveIndex = INDEX_NONE;
    LastCompletedObjectiveIndex = INDEX_NONE;
    SetMissionState(EAscMissionState::Inactive);
    ForceNetUpdate();
}

AAscObjectiveBase* AAscMissionDirector::GetCurrentObjective() const
{
    return Objectives.IsValidIndex(CurrentObjectiveIndex)
        ? Objectives[CurrentObjectiveIndex]
        : nullptr;
}

bool AAscMissionDirector::IsFinalObjective(AAscObjectiveBase* Objective) const
{
    return Objective != nullptr
        && !Objectives.IsEmpty()
        && Objectives.Last() == Objective;
}

void AAscMissionDirector::HandleObjectiveCompleted(AAscObjectiveBase* Objective)
{
    if (!HasAuthority() || MissionState != EAscMissionState::Running)
    {
        return;
    }

    if (Objective != GetCurrentObjective())
    {
        return;
    }

    // Replicate completion independently from CurrentObjectiveIndex. The director
    // advances immediately, so client UI must not depend on still being bound to
    // the completed objective when its State replication arrives.
    LastCompletedObjectiveIndex = CurrentObjectiveIndex;
    OnObjectiveCompleted.Broadcast(this, Objective);
    ForceNetUpdate();

    AdvanceObjective();
}

void AAscMissionDirector::HandleObjectiveFailed(AAscObjectiveBase* Objective)
{
    if (!HasAuthority() || MissionState != EAscMissionState::Running)
    {
        return;
    }

    if (Objective != GetCurrentObjective())
    {
        return;
    }

    FailMission();
}

void AAscMissionDirector::OnRep_MissionState(EAscMissionState OldState)
{
    BroadcastMissionStateTransition(OldState, MissionState);
}

void AAscMissionDirector::OnRep_CurrentObjectiveIndex(int32 OldIndex)
{
    if (OldIndex != CurrentObjectiveIndex)
    {
        OnObjectiveChanged.Broadcast(this, GetCurrentObjective());
    }
}

void AAscMissionDirector::OnRep_LastCompletedObjectiveIndex(int32 OldIndex)
{
    if (OldIndex == LastCompletedObjectiveIndex || LastCompletedObjectiveIndex == INDEX_NONE)
    {
        return;
    }

    if (Objectives.IsValidIndex(LastCompletedObjectiveIndex))
    {
        OnObjectiveCompleted.Broadcast(this, Objectives[LastCompletedObjectiveIndex]);
    }
}

void AAscMissionDirector::BindObjectiveEvents()
{
    for (AAscObjectiveBase* Objective : Objectives)
    {
        if (!Objective)
        {
            continue;
        }

        Objective->OnObjectiveCompleted.RemoveDynamic(this, &AAscMissionDirector::HandleObjectiveCompleted);
        Objective->OnObjectiveFailed.RemoveDynamic(this, &AAscMissionDirector::HandleObjectiveFailed);
        Objective->OnObjectiveCompleted.AddDynamic(this, &AAscMissionDirector::HandleObjectiveCompleted);
        Objective->OnObjectiveFailed.AddDynamic(this, &AAscMissionDirector::HandleObjectiveFailed);
    }
}

void AAscMissionDirector::ActivateCurrentObjective()
{
    AAscObjectiveBase* Objective = GetCurrentObjective();
    if (!Objective)
    {
        CompleteMission();
        return;
    }

    Objective->ActivateObjective();
}

void AAscMissionDirector::AdvanceObjective()
{
    ++CurrentObjectiveIndex;
    ForceNetUpdate();

    if (!Objectives.IsValidIndex(CurrentObjectiveIndex))
    {
        CompleteMission();
        return;
    }

    OnObjectiveChanged.Broadcast(this, GetCurrentObjective());
    ActivateCurrentObjective();
}

void AAscMissionDirector::CompleteMission()
{
    if (!HasAuthority())
    {
        return;
    }

    CurrentObjectiveIndex = INDEX_NONE;
    SetMissionState(EAscMissionState::Completed);
    ForceNetUpdate();
}

void AAscMissionDirector::FailMission()
{
    if (!HasAuthority() || MissionState != EAscMissionState::Running)
    {
        return;
    }

    SetMissionState(EAscMissionState::Failed);
    ForceNetUpdate();
}

void AAscMissionDirector::SetMissionState(EAscMissionState NewState)
{
    if (MissionState == NewState)
    {
        return;
    }

    const EAscMissionState OldState = MissionState;
    MissionState = NewState;
    BroadcastMissionStateTransition(OldState, NewState);
}

void AAscMissionDirector::BroadcastMissionStateTransition(
    EAscMissionState OldState,
    EAscMissionState NewState)
{
    OnMissionStateChanged.Broadcast(this, OldState, NewState);

    switch (NewState)
    {
    case EAscMissionState::Running:
        OnMissionStarted.Broadcast(this);
        break;
    case EAscMissionState::Completed:
        OnMissionCompleted.Broadcast(this);
        break;
    case EAscMissionState::Failed:
        OnMissionFailed.Broadcast(this);
        break;
    default:
        break;
    }
}
