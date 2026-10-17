#include "AscTargetHealthBarWidget.h"

void UAscTargetHealthBarWidget::NativePreConstruct()
{
    ApplyRelationStyle();
    Super::NativePreConstruct();
}

void UAscTargetHealthBarWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    ApplyRelationStyle();
}

void UAscTargetHealthBarWidget::SetHealthSnapshot(
    const FAscenduntHealthSnapshot& InSnapshot)
{
    // The display component only pushes a new snapshot when target status changes.
    // The shared widget confirms an actual total-health decrease before playing feedback.
    ApplyHealthSnapshot(InSnapshot, true);
    BP_OnHealthSnapshotChanged(InSnapshot, GetDisplayedHealthPercent());
}

void UAscTargetHealthBarWidget::SetRelation(
    EAscTargetHealthBarRelation InRelation)
{
    if (CurrentRelation != InRelation)
    {
        CurrentRelation = InRelation;
        ApplyRelationStyle();
    }

    BP_OnRelationChanged(CurrentRelation);
}

void UAscTargetHealthBarWidget::ApplyRelationStyle()
{
    ApplyStyleContext(
        HealthBarStyleSet,
        CurrentRelation == EAscTargetHealthBarRelation::Ally
            ? EAscHealthBarStyleContext::FriendlyTarget
            : EAscHealthBarStyleContext::EnemyTarget);
}
