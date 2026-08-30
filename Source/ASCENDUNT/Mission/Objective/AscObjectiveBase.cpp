#include "ASCENDUNT/Mission/Objective/AscObjectiveBase.h"

#include "ASCENDUNT/Mission/Conditions/AscObjectiveConditionComponent.h"
#include "Net/UnrealNetwork.h"

AAscObjectiveBase::AAscObjectiveBase()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(false);
}

void AAscObjectiveBase::BeginPlay()
{
    Super::BeginPlay();
    InitializeConditionComponents();
}

void AAscObjectiveBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AAscObjectiveBase, State);
    DOREPLIFETIME(AAscObjectiveBase, ConditionSnapshots);
}

void AAscObjectiveBase::ActivateObjective()
{
    if (!HasAuthority() || State != EAscObjectiveState::Inactive)
    {
        return;
    }

    InitializeConditionComponents();
    SetState(EAscObjectiveState::Active);
    ActivateConditions();
    RefreshAllConditionSnapshots();
    EvaluateObjective();
}

void AAscObjectiveBase::DeactivateObjective()
{
    if (!HasAuthority())
    {
        return;
    }

    bSuppressConditionEvaluation = true;
    DeactivateConditions();

    if (State == EAscObjectiveState::Active)
    {
        SetState(EAscObjectiveState::Inactive);
    }
    bSuppressConditionEvaluation = false;
}

void AAscObjectiveBase::ResetObjective()
{
    if (!HasAuthority())
    {
        return;
    }

    InitializeConditionComponents();

    bSuppressConditionEvaluation = true;
    for (UAscObjectiveConditionComponent* Condition : AllConditionComponents)
    {
        if (Condition)
        {
            Condition->ResetCondition();
        }
    }

    SetState(EAscObjectiveState::Inactive);
    bSuppressConditionEvaluation = false;
    RefreshAllConditionSnapshots();
}

bool AAscObjectiveBase::GetConditionSnapshot(
    FGameplayTag InConditionId,
    FAscObjectiveConditionSnapshot& OutSnapshot) const
{
    for (const FAscObjectiveConditionSnapshot& Snapshot : ConditionSnapshots)
    {
        if (Snapshot.ConditionId == InConditionId)
        {
            OutSnapshot = Snapshot;
            return true;
        }
    }

    return false;
}

UAscObjectiveConditionComponent* AAscObjectiveBase::GetConditionComponent(FGameplayTag InConditionId) const
{
    for (UAscObjectiveConditionComponent* Condition : AllConditionComponents)
    {
        if (Condition && Condition->GetConditionId() == InConditionId)
        {
            return Condition;
        }
    }

    return nullptr;
}

void AAscObjectiveBase::CompleteObjective()
{
    if (!HasAuthority() || State != EAscObjectiveState::Active)
    {
        return;
    }

    bSuppressConditionEvaluation = true;
    DeactivateConditions();
    SetState(EAscObjectiveState::Completed);
    bSuppressConditionEvaluation = false;
    RefreshAllConditionSnapshots();
}

void AAscObjectiveBase::FailObjective()
{
    if (!HasAuthority() || State != EAscObjectiveState::Active)
    {
        return;
    }

    bSuppressConditionEvaluation = true;
    DeactivateConditions();
    SetState(EAscObjectiveState::Failed);
    bSuppressConditionEvaluation = false;
    RefreshAllConditionSnapshots();
}

void AAscObjectiveBase::HandleConditionStateChanged(
    UAscObjectiveConditionComponent* Condition,
    EAscObjectiveConditionState OldState,
    EAscObjectiveConditionState NewState)
{
    if (!HasAuthority() || !Condition)
    {
        return;
    }

    UpdateConditionSnapshot(Condition);

    OnConditionStateChanged.Broadcast(this, Condition->GetConditionId(), OldState, NewState);

    if (NewState == EAscObjectiveConditionState::Satisfied)
    {
        OnConditionSatisfied.Broadcast(this, Condition->GetConditionId());
    }
    else if (NewState == EAscObjectiveConditionState::Failed)
    {
        OnConditionFailed.Broadcast(this, Condition->GetConditionId());
    }

    if (!bSuppressConditionEvaluation)
    {
        EvaluateObjective();
    }
}

void AAscObjectiveBase::HandleConditionProgressChanged(
    UAscObjectiveConditionComponent* Condition,
    float CurrentValue,
    float TargetValue)
{
    if (!HasAuthority() || !Condition)
    {
        return;
    }

    UpdateConditionSnapshot(Condition);
    OnConditionProgressChanged.Broadcast(
        this,
        Condition->GetConditionId(),
        CurrentValue,
        TargetValue);
}

void AAscObjectiveBase::OnRep_State(EAscObjectiveState OldState)
{
    BroadcastObjectiveStateTransition(OldState, State);
}

void AAscObjectiveBase::OnRep_ConditionSnapshots()
{
    // Components are created from the Actor/BP on clients too. Refresh the cache
    // before applying replicated snapshots in case this is the first update.
    if (AllConditionComponents.IsEmpty())
    {
        InitializeConditionComponents();
    }

    BroadcastConditionSnapshotDiffs();
    LastNotifiedConditionSnapshots = ConditionSnapshots;
}

void AAscObjectiveBase::InitializeConditionComponents()
{
    AllConditionComponents.Reset();
    CompletionConditions.Reset();
    FailureConditions.Reset();

    TArray<UAscObjectiveConditionComponent*> FoundConditions;
    GetComponents<UAscObjectiveConditionComponent>(FoundConditions);

    TSet<FGameplayTag> SeenConditionIds;

    for (UAscObjectiveConditionComponent* Condition : FoundConditions)
    {
        if (!Condition)
        {
            continue;
        }

        AllConditionComponents.Add(Condition);

        if (Condition->GetConditionRole() == EAscObjectiveConditionRole::Failure)
        {
            FailureConditions.Add(Condition);
        }
        else
        {
            CompletionConditions.Add(Condition);
        }

        if (HasAuthority())
        {
            Condition->OnConditionStateChanged.RemoveDynamic(this, &AAscObjectiveBase::HandleConditionStateChanged);
            Condition->OnConditionProgressChanged.RemoveDynamic(this, &AAscObjectiveBase::HandleConditionProgressChanged);
            Condition->OnConditionStateChanged.AddDynamic(this, &AAscObjectiveBase::HandleConditionStateChanged);
            Condition->OnConditionProgressChanged.AddDynamic(this, &AAscObjectiveBase::HandleConditionProgressChanged);
        }

        const FGameplayTag ConditionId = Condition->GetConditionId();
        if (!ConditionId.IsValid())
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[Mission] Condition component '%s' in objective '%s' has no ConditionId."),
                *Condition->GetName(),
                *GetName());
            continue;
        }

        if (SeenConditionIds.Contains(ConditionId))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[Mission] Duplicate ConditionId '%s' in objective '%s'."),
                *ConditionId.ToString(),
                *GetName());
        }

        SeenConditionIds.Add(ConditionId);
    }
}

void AAscObjectiveBase::ActivateConditions()
{
    // Arm failure triggers first so an immediately true failure condition cannot
    // lose to an immediately true completion condition in the same activation.
    for (UAscObjectiveConditionComponent* Condition : FailureConditions)
    {
        if (State != EAscObjectiveState::Active)
        {
            return;
        }

        if (Condition)
        {
            Condition->ActivateCondition();
        }
    }

    for (UAscObjectiveConditionComponent* Condition : CompletionConditions)
    {
        if (State != EAscObjectiveState::Active)
        {
            return;
        }

        if (Condition)
        {
            Condition->ActivateCondition();
        }
    }
}

void AAscObjectiveBase::DeactivateConditions()
{
    for (UAscObjectiveConditionComponent* Condition : AllConditionComponents)
    {
        if (Condition)
        {
            Condition->DeactivateCondition();
        }
    }
}

void AAscObjectiveBase::EvaluateObjective()
{
    if (!HasAuthority() || State != EAscObjectiveState::Active)
    {
        return;
    }

    // Failure triggers have priority if success/failure happen in the same frame.
    if (!FailureConditions.IsEmpty() && AreConditionsSatisfied(FailureConditions, FailurePolicy))
    {
        FailObjective();
        return;
    }

    // A completion condition entering Failed means a required success path may
    // have become impossible.
    if (AreCompletionConditionsImpossible())
    {
        FailObjective();
        return;
    }

    if (!CompletionConditions.IsEmpty() && AreConditionsSatisfied(CompletionConditions, CompletionPolicy))
    {
        CompleteObjective();
    }
}

bool AAscObjectiveBase::AreConditionsSatisfied(
    const TArray<TObjectPtr<UAscObjectiveConditionComponent>>& Conditions,
    EAscObjectiveConditionPolicy Policy) const
{
    if (Conditions.IsEmpty())
    {
        return false;
    }

    if (Policy == EAscObjectiveConditionPolicy::All)
    {
        for (const UAscObjectiveConditionComponent* Condition : Conditions)
        {
            if (!Condition || !Condition->IsConditionSatisfied())
            {
                return false;
            }
        }
        return true;
    }

    for (const UAscObjectiveConditionComponent* Condition : Conditions)
    {
        if (Condition && Condition->IsConditionSatisfied())
        {
            return true;
        }
    }

    return false;
}

bool AAscObjectiveBase::AreCompletionConditionsImpossible() const
{
    if (CompletionConditions.IsEmpty())
    {
        return false;
    }

    if (CompletionPolicy == EAscObjectiveConditionPolicy::All)
    {
        for (const UAscObjectiveConditionComponent* Condition : CompletionConditions)
        {
            if (Condition && Condition->IsConditionFailed())
            {
                return true;
            }
        }
        return false;
    }

    // Any-policy only becomes impossible after every completion candidate failed.
    for (const UAscObjectiveConditionComponent* Condition : CompletionConditions)
    {
        if (Condition && !Condition->IsConditionFailed())
        {
            return false;
        }
    }

    return true;
}

void AAscObjectiveBase::SetState(EAscObjectiveState NewState)
{
    if (State == NewState)
    {
        return;
    }

    const EAscObjectiveState OldState = State;
    State = NewState;
    BroadcastObjectiveStateTransition(OldState, NewState);
    ForceNetUpdate();
}

void AAscObjectiveBase::RefreshAllConditionSnapshots()
{
    if (!HasAuthority())
    {
        return;
    }

    ConditionSnapshots.Reset();

    for (UAscObjectiveConditionComponent* Condition : AllConditionComponents)
    {
        if (!Condition || !Condition->GetConditionId().IsValid())
        {
            continue;
        }

        FAscObjectiveConditionSnapshot Snapshot;
        Snapshot.ConditionId = Condition->GetConditionId();
        Snapshot.State = Condition->GetConditionState();
        Snapshot.CurrentValue = Condition->GetCurrentProgressValue();
        Snapshot.TargetValue = Condition->GetTargetProgressValue();
        ConditionSnapshots.Add(Snapshot);
    }

    LastNotifiedConditionSnapshots = ConditionSnapshots;
    ForceNetUpdate();
}

void AAscObjectiveBase::UpdateConditionSnapshot(UAscObjectiveConditionComponent* Condition)
{
    if (!HasAuthority() || !Condition || !Condition->GetConditionId().IsValid())
    {
        return;
    }

    FAscObjectiveConditionSnapshot* Existing = ConditionSnapshots.FindByPredicate(
        [Condition](const FAscObjectiveConditionSnapshot& Snapshot)
        {
            return Snapshot.ConditionId == Condition->GetConditionId();
        });

    if (!Existing)
    {
        FAscObjectiveConditionSnapshot Snapshot;
        Snapshot.ConditionId = Condition->GetConditionId();
        Snapshot.State = Condition->GetConditionState();
        Snapshot.CurrentValue = Condition->GetCurrentProgressValue();
        Snapshot.TargetValue = Condition->GetTargetProgressValue();
        ConditionSnapshots.Add(Snapshot);
    }
    else
    {
        Existing->State = Condition->GetConditionState();
        Existing->CurrentValue = Condition->GetCurrentProgressValue();
        Existing->TargetValue = Condition->GetTargetProgressValue();
    }

    LastNotifiedConditionSnapshots = ConditionSnapshots;
    ForceNetUpdate();
}

void AAscObjectiveBase::BroadcastObjectiveStateTransition(
    EAscObjectiveState OldState,
    EAscObjectiveState NewState)
{
    OnObjectiveStateChanged.Broadcast(this, OldState, NewState);

    switch (NewState)
    {
    case EAscObjectiveState::Active:
        OnObjectiveActivated.Broadcast(this);
        break;
    case EAscObjectiveState::Completed:
        OnObjectiveCompleted.Broadcast(this);
        break;
    case EAscObjectiveState::Failed:
        OnObjectiveFailed.Broadcast(this);
        break;
    default:
        break;
    }
}

void AAscObjectiveBase::BroadcastConditionSnapshotDiffs()
{
    for (const FAscObjectiveConditionSnapshot& NewSnapshot : ConditionSnapshots)
    {
        const FAscObjectiveConditionSnapshot* OldSnapshot = LastNotifiedConditionSnapshots.FindByPredicate(
            [&NewSnapshot](const FAscObjectiveConditionSnapshot& Snapshot)
            {
                return Snapshot.ConditionId == NewSnapshot.ConditionId;
            });

        const EAscObjectiveConditionState OldState = OldSnapshot
            ? OldSnapshot->State
            : EAscObjectiveConditionState::Inactive;

        if (UAscObjectiveConditionComponent* Condition = GetConditionComponent(NewSnapshot.ConditionId))
        {
            // Replays state/progress changes through the component's own BP delegates
            // on clients, while the authoritative component logic remains server-only.
            Condition->ApplyReplicatedSnapshot(NewSnapshot);
        }

        if (!OldSnapshot || OldState != NewSnapshot.State)
        {
            OnConditionStateChanged.Broadcast(
                this,
                NewSnapshot.ConditionId,
                OldState,
                NewSnapshot.State);

            if (NewSnapshot.State == EAscObjectiveConditionState::Satisfied)
            {
                OnConditionSatisfied.Broadcast(this, NewSnapshot.ConditionId);
            }
            else if (NewSnapshot.State == EAscObjectiveConditionState::Failed)
            {
                OnConditionFailed.Broadcast(this, NewSnapshot.ConditionId);
            }
        }

        if (!OldSnapshot
            || !FMath::IsNearlyEqual(OldSnapshot->CurrentValue, NewSnapshot.CurrentValue)
            || !FMath::IsNearlyEqual(OldSnapshot->TargetValue, NewSnapshot.TargetValue))
        {
            OnConditionProgressChanged.Broadcast(
                this,
                NewSnapshot.ConditionId,
                NewSnapshot.CurrentValue,
                NewSnapshot.TargetValue);
        }
    }
}
