#include "AscHealthBarStyleData.h"

#include "ASCENDUNT/UI/InGame/HealthBar/AscHealthSegmentWidget.h"

FAscHealthBarStyle::FAscHealthBarStyle()
{
    SegmentWidgetClass = UAscHealthSegmentWidget::StaticClass();
}

UAscHealthBarStyleSetData::UAscHealthBarStyleSetData(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PlayerHUD = MakeDefaultStyle(EAscHealthBarStyleContext::PlayerHUD);
    FriendlyTarget = MakeDefaultStyle(EAscHealthBarStyleContext::FriendlyTarget);
    EnemyTarget = MakeDefaultStyle(EAscHealthBarStyleContext::EnemyTarget);
}

const FAscHealthBarStyle& UAscHealthBarStyleSetData::GetStyle(
    EAscHealthBarStyleContext Context) const
{
    switch (Context)
    {
    case EAscHealthBarStyleContext::PlayerHUD:
        return PlayerHUD;

    case EAscHealthBarStyleContext::FriendlyTarget:
        return FriendlyTarget;

    case EAscHealthBarStyleContext::EnemyTarget:
    default:
        return EnemyTarget;
    }
}

FAscHealthBarStyle UAscHealthBarStyleSetData::MakeDefaultStyle(
    EAscHealthBarStyleContext Context)
{
    FAscHealthBarStyle Style;

    switch (Context)
    {
    case EAscHealthBarStyleContext::PlayerHUD:
        // Tuned values from the PlayerHealthBar instance in the current InGameHUD.
        Style.VisualMode = EAscSegmentedHealthBarVisualMode::FriendlyDetailed;
        Style.HealthBarWidth = 300.0f;
        Style.HealthBarHeight = 48.0f;
        Style.MinSegmentWidth = 3.0f;
        Style.SegmentHeight = 48.0f;
        Style.SegmentGap = 2.0f;
        Style.PoolGap = 2.0f;
        Style.OverHealthColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);
        Style.DamagePulseColor = FLinearColor::Red;
        Style.DamagePulseDrawSize = FVector2D(16.0f, 32.0f);
        Style.DamagePulsePivot = FVector2D(0.0f, 0.5f);
        Style.DamagePulseStartScale = FVector2D(1.0f, 1.0f);
        Style.DamagePulseEndScale = FVector2D(1.0f, 3.0f);
        break;

    case EAscHealthBarStyleContext::FriendlyTarget:
        // Tuned values from WBP_TargetHealthBar.
        Style.VisualMode = EAscSegmentedHealthBarVisualMode::FriendlyDetailed;
        Style.HealthBarWidth = 80.0f;
        Style.HealthBarHeight = 40.0f;
        Style.MinSegmentWidth = 3.0f;
        Style.SegmentHeight = 10.0f;
        Style.SegmentGap = 1.0f;
        Style.PoolGap = 1.0f;
        Style.OverHealthColor = FLinearColor(0.05f, 0.28f, 1.0f, 1.0f);
        Style.DamagePulseColor = FLinearColor::White;
        Style.DamagePulseDrawSize = FVector2D(4.0f, 10.0f);
        Style.DamagePulsePivot = FVector2D(0.0f, 0.5f);
        Style.DamagePulseStartScale = FVector2D(1.0f, 4.0f);
        Style.DamagePulseEndScale = FVector2D(1.0f, 1.0f);
        break;

    case EAscHealthBarStyleContext::EnemyTarget:
    default:
        Style = MakeDefaultStyle(EAscHealthBarStyleContext::FriendlyTarget);
        Style.VisualMode = EAscSegmentedHealthBarVisualMode::EnemyFlatRed;
        break;
    }

    return Style;
}
