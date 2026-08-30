#include "ASCENDUNT/Mission/Conditions/AscObjectiveConditionComponent.h"

#include "ASCENDUNT/Mission/Objective/AscObjectiveBase.h"
#include "GameFramework/Actor.h"

UAscObjectiveConditionComponent::UAscObjectiveConditionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(false);
}

void UAscObjectiveConditionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (const AActor* Owner = GetOwner(); Owner && Owner->HasAuthority())
    {
        OnDeactivated();
    }

    Super::EndPlay(EndPlayReason);
}

void UAscObjectiveConditionComponent::ActivateCondition()
{
    const AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        return;
    }

    if (State == EAscObjectiveConditionState::Active
        || State == EAscObjectiveConditionState::Satisfied
        || State == EAscObjectiveConditionState::Failed)
    {
        return;
    }

    SetState(EAscObjectiveConditionState::Active);
    OnConditionActivated.Broadcast(this);
    OnActivated();
    NotifyProgressChanged();
}

void UAscObjectiveConditionComponent::DeactivateCondition()
{
    const AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        return;
    }

    OnDeactivated();

    if (State == EAscObjectiveConditionState::Active)
    {
        SetState(EAscObjectiveConditionState::Inactive);
    }
}

void UAscObjectiveConditionComponent::ResetCondition()
{
    const AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        return;
    }

    OnDeactivated();
    OnReset();
    SetState(EAscObjectiveConditionState::Inactive);
    NotifyProgressChanged();
}

AAscObjectiveBase* UAscObjectiveConditionComponent::GetOwningObjective() const
{
    return Cast<AAscObjectiveBase>(GetOwner());
}

float UAscObjectiveConditionComponent::GetCurrentProgressValue_Implementation() const
{
    return IsConditionSatisfied() ? 1.0f : 0.0f;
}

float UAscObjectiveConditionComponent::GetTargetProgressValue_Implementation() const
{
    return 1.0f;
}

void UAscObjectiveConditionComponent::Satisfy()
{
    const AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority() || State != EAscObjectiveConditionState::Active)
    {
        return;
    }

    SetState(EAscObjectiveConditionState::Satisfied);
    NotifyProgressChanged();
    OnConditionSatisfied.Broadcast(this);
}

void UAscObjectiveConditionComponent::Fail()
{
    const AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority() || State != EAscObjectiveConditionState::Active)
    {
        return;
    }

    SetState(EAscObjectiveConditionState::Failed);
    NotifyProgressChanged();
    OnConditionFailed.Broadcast(this);
}

void UAscObjectiveConditionComponent::NotifyProgressChanged()
{
    OnConditionProgressChanged.Broadcast(
        this,
        GetCurrentProgressValue(),
        GetTargetProgressValue());
}

void UAscObjectiveConditionComponent::ApplyReplicatedSnapshot(
    const FAscObjectiveConditionSnapshot& Snapshot)
{
    const EAscObjectiveConditionState OldState = State;
    const bool bStateChanged = OldState != Snapshot.State;
    const bool bProgressChanged =
        !FMath::IsNearlyEqual(LastReplicatedCurrentValue, Snapshot.CurrentValue)
        || !FMath::IsNearlyEqual(LastReplicatedTargetValue, Snapshot.TargetValue);

    State = Snapshot.State;
    LastReplicatedCurrentValue = Snapshot.CurrentValue;
    LastReplicatedTargetValue = Snapshot.TargetValue;

    if (bStateChanged)
    {
        OnConditionStateChanged.Broadcast(this, OldState, State);

        if (State == EAscObjectiveConditionState::Active)
        {
            OnConditionActivated.Broadcast(this);
        }
        else if (State == EAscObjectiveConditionState::Satisfied)
        {
            OnConditionSatisfied.Broadcast(this);
        }
        else if (State == EAscObjectiveConditionState::Failed)
        {
            OnConditionFailed.Broadcast(this);
        }
    }

    if (bProgressChanged)
    {
        OnConditionProgressChanged.Broadcast(
            this,
            Snapshot.CurrentValue,
            Snapshot.TargetValue);
    }
}

void UAscObjectiveConditionComponent::SetState(EAscObjectiveConditionState NewState)
{
    if (State == NewState)
    {
        return;
    }

    const EAscObjectiveConditionState OldState = State;
    State = NewState;
    OnConditionStateChanged.Broadcast(this, OldState, NewState);
}
