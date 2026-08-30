#include "AscSegmentedHealthBarWidget.h"

#include "ASCENDUNT/UI/InGame/HealthBar/AscHealthSegmentWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"

UAscSegmentedHealthBarWidget::UAscSegmentedHealthBarWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    ActiveStyle = FAscHealthBarStyle();
}

void UAscSegmentedHealthBarWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyEditorConfiguration();
}

void UAscSegmentedHealthBarWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    EnsureNativeFallbackWidget();
    ApplyEditorConfiguration();
    StopDamagePulse();
    RebuildSegments();
    RefreshSegments();
}

void UAscSegmentedHealthBarWidget::NativeTick(
    const FGeometry& MyGeometry,
    float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!bDamagePulseActive)
    {
        return;
    }

    DamagePulseElapsed += InDeltaTime;
    const float NormalizedTime = ActiveStyle.DamagePulseDuration > KINDA_SMALL_NUMBER
        ? FMath::Clamp(DamagePulseElapsed / ActiveStyle.DamagePulseDuration, 0.0f, 1.0f)
        : 1.0f;

    UpdateDamagePulseVisual(NormalizedTime);

    if (NormalizedTime >= 1.0f)
    {
        StopDamagePulse();
    }
}

void UAscSegmentedHealthBarWidget::ApplyHealthSnapshot(
    const FAscenduntHealthSnapshot& InSnapshot,
    bool bWasDamaging)
{
    const float PreviousTotal = bHasSnapshot ? CurrentSnapshot.GetCurrentTotal() : 0.0f;

    CurrentSnapshot = InSnapshot;
    UpdateDynamicOverHealthCapacity();

    if (DoesLayoutNeedRebuild())
    {
        RebuildSegments();
    }

    RefreshSegments();

    const float CurrentTotal = CurrentSnapshot.GetCurrentTotal();
    const bool bActuallyLostHealth =
        bHasSnapshot &&
        bWasDamaging &&
        CurrentTotal < PreviousTotal - KINDA_SMALL_NUMBER;

    bHasSnapshot = true;

    if (bActuallyLostHealth)
    {
        PlayDamagePulse();
    }

    BP_OnSegmentedHealthChanged(CurrentSnapshot, DisplayedHealthPercent);
}

void UAscSegmentedHealthBarWidget::SetVisualMode(
    EAscSegmentedHealthBarVisualMode InVisualMode)
{
    if (VisualMode == InVisualMode)
    {
        return;
    }

    VisualMode = InVisualMode;
    ActiveStyle.VisualMode = InVisualMode;

    if (HB_Segments)
    {
        RebuildSegments();
        RefreshSegments();
    }
}

void UAscSegmentedHealthBarWidget::ApplyStyle(const FAscHealthBarStyle& InStyle)
{
    ActiveStyle = InStyle;
    if (!ActiveStyle.SegmentWidgetClass)
    {
        ActiveStyle.SegmentWidgetClass = UAscHealthSegmentWidget::StaticClass();
    }

    VisualMode = ActiveStyle.VisualMode;
    CachedConfiguredWidth = -1.0f;

    ApplyEditorConfiguration();
    StopDamagePulse();

    if (HB_Segments)
    {
        RebuildSegments();
        RefreshSegments();
    }
}

void UAscSegmentedHealthBarWidget::ApplyStyleContext(
    const UAscHealthBarStyleSetData* StyleSet,
    EAscHealthBarStyleContext Context)
{
    ApplyStyle(
        StyleSet
            ? StyleSet->GetStyle(Context)
            : UAscHealthBarStyleSetData::MakeDefaultStyle(Context));
}

void UAscSegmentedHealthBarWidget::SetPresentationScale(float InPresentationScale)
{
    const float NewScale = FMath::Clamp(InPresentationScale, 1.0f, 8.0f);
    if (FMath::IsNearlyEqual(PresentationScale, NewScale))
    {
        return;
    }

    PresentationScale = NewScale;
    CachedConfiguredWidth = -1.0f;

    ApplyEditorConfiguration();
    StopDamagePulse();

    if (HB_Segments)
    {
        RebuildSegments();
        RefreshSegments();
    }
}

void UAscSegmentedHealthBarWidget::ResetHealthDisplay()
{
    CurrentSnapshot = FAscenduntHealthSnapshot();
    DynamicOverHealthCapacity = 0.0f;
    DisplayedHealthPercent = 0.0f;
    bHasSnapshot = false;

    CachedThornCapacity = -1.0f;
    CachedBasicCapacity = -1.0f;
    CachedArmorCapacity = -1.0f;
    CachedOverHealthCapacity = -1.0f;
    CachedEnemyTotalCapacity = -1.0f;
    CachedConfiguredWidth = -1.0f;

    StopDamagePulse();
    RebuildSegments();
    RefreshSegments();
}

void UAscSegmentedHealthBarWidget::EnsureNativeFallbackWidget()
{
    if (HB_Segments && CP_DamageEffects && Img_DamagePulse)
    {
        return;
    }

    if (!WidgetTree)
    {
        return;
    }

    if (WidgetTree->RootWidget)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[%s] Missing HB_Segments / CP_DamageEffects / Img_DamagePulse bindings. Replacing the unusable root with the native segmented hierarchy."),
            *GetNameSafe(this));
    }

    SB_HealthBar = WidgetTree->ConstructWidget<USizeBox>(
        USizeBox::StaticClass(), TEXT("SB_HealthBar"));
    SB_HealthBar->SetWidthOverride(GetRenderedHealthBarWidth());
    SB_HealthBar->SetHeightOverride(GetRenderedHealthBarHeight());

    UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(
        UOverlay::StaticClass(), TEXT("SegmentedHealthRoot"));
    SB_HealthBar->SetContent(RootOverlay);

    HB_Segments = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), TEXT("HB_Segments"));

    if (UOverlaySlot* SegmentsSlot = RootOverlay->AddChildToOverlay(HB_Segments))
    {
        SegmentsSlot->SetHorizontalAlignment(HAlign_Center);
        SegmentsSlot->SetVerticalAlignment(VAlign_Center);
    }

    CP_DamageEffects = WidgetTree->ConstructWidget<UCanvasPanel>(
        UCanvasPanel::StaticClass(), TEXT("CP_DamageEffects"));
    CP_DamageEffects->SetVisibility(ESlateVisibility::HitTestInvisible);

    if (UOverlaySlot* EffectsSlot = RootOverlay->AddChildToOverlay(CP_DamageEffects))
    {
        EffectsSlot->SetHorizontalAlignment(HAlign_Fill);
        EffectsSlot->SetVerticalAlignment(VAlign_Fill);
    }

    Img_DamagePulse = WidgetTree->ConstructWidget<UImage>(
        UImage::StaticClass(), TEXT("Img_DamagePulse"));

    if (UCanvasPanelSlot* DamageSlot = CP_DamageEffects->AddChildToCanvas(Img_DamagePulse))
    {
        DamageSlot->SetAnchors(FAnchors(0.0f, 0.0f));
        DamageSlot->SetAlignment(ActiveStyle.DamagePulsePivot);
        DamageSlot->SetAutoSize(false);
        DamageSlot->SetSize(ActiveStyle.DamagePulseDrawSize * GetPresentationScale());
        DamageSlot->SetPosition(FVector2D(0.0f, GetRenderedHealthBarHeight() * 0.5f));
    }

    WidgetTree->RootWidget = SB_HealthBar;
    ApplyEditorConfiguration();
}

void UAscSegmentedHealthBarWidget::ApplyEditorConfiguration()
{
    if (SB_HealthBar)
    {
        SB_HealthBar->SetWidthOverride(GetRenderedHealthBarWidth());
        SB_HealthBar->SetHeightOverride(GetRenderedHealthBarHeight());
    }

    if (!Img_DamagePulse)
    {
        return;
    }

    if (ActiveStyle.DamagePulseTexture)
    {
        Img_DamagePulse->SetBrushFromTexture(ActiveStyle.DamagePulseTexture, false);
    }

    Img_DamagePulse->SetColorAndOpacity(ActiveStyle.DamagePulseColor);
    Img_DamagePulse->SetRenderTransformPivot(ActiveStyle.DamagePulsePivot);
    Img_DamagePulse->SetRenderTransformAngle(ActiveStyle.DamagePulseRenderAngle);

    if (UCanvasPanelSlot* DamageSlot = Cast<UCanvasPanelSlot>(Img_DamagePulse->Slot))
    {
        DamageSlot->SetAutoSize(false);
        DamageSlot->SetAlignment(ActiveStyle.DamagePulsePivot);
        DamageSlot->SetAnchors(FAnchors(0.0f, 0.0f));
        DamageSlot->SetSize(FVector2D(
            FMath::Max(1.0f, ActiveStyle.DamagePulseDrawSize.X * GetPresentationScale()),
            FMath::Max(1.0f, ActiveStyle.DamagePulseDrawSize.Y * GetPresentationScale())));
    }
}

void UAscSegmentedHealthBarWidget::UpdateDynamicOverHealthCapacity()
{
    const float DeclaredCapacity = FMath::Max(0.0f, CurrentSnapshot.MaxOverHealth);
    const float CurrentOverHealth = FMath::Max(0.0f, CurrentSnapshot.OverHealth);

    if (CurrentOverHealth > KINDA_SMALL_NUMBER)
    {
        // When MaxOverHealth is not configured, retain the peak temporary-health
        // capacity while it is being depleted instead of shrinking every hit.
        DynamicOverHealthCapacity = FMath::Max(
            DynamicOverHealthCapacity,
            FMath::Max(DeclaredCapacity, CurrentOverHealth));
    }
    else
    {
        DynamicOverHealthCapacity = DeclaredCapacity;
    }
}

bool UAscSegmentedHealthBarWidget::DoesLayoutNeedRebuild() const
{
    if (!HB_Segments ||
        CachedLayoutMode != VisualMode ||
        !FMath::IsNearlyEqual(CachedConfiguredWidth, GetRenderedHealthBarWidth()))
    {
        return true;
    }

    if (VisualMode == EAscSegmentedHealthBarVisualMode::EnemyFlatRed)
    {
        return !FMath::IsNearlyEqual(
            CachedEnemyTotalCapacity,
            GetDisplayCapacityTotal());
    }

    return
        !FMath::IsNearlyEqual(CachedThornCapacity, GetFriendlyPoolCapacity(EAscHealthPoolType::Thorn)) ||
        !FMath::IsNearlyEqual(CachedBasicCapacity, GetFriendlyPoolCapacity(EAscHealthPoolType::BasicHealth)) ||
        !FMath::IsNearlyEqual(CachedArmorCapacity, GetFriendlyPoolCapacity(EAscHealthPoolType::Armor)) ||
        !FMath::IsNearlyEqual(CachedOverHealthCapacity, GetFriendlyPoolCapacity(EAscHealthPoolType::OverHealth));
}

void UAscSegmentedHealthBarWidget::RebuildSegments()
{
    EnsureNativeFallbackWidget();

    if (!HB_Segments || !ActiveStyle.SegmentWidgetClass || !WidgetTree)
    {
        return;
    }

    HB_Segments->ClearChildren();
    RuntimeSegments.Reset();
    CachedBarWidth = 0.0f;
    CalculateResolvedLayoutMetrics();

    if (VisualMode == EAscSegmentedHealthBarVisualMode::EnemyFlatRed)
    {
        AddEnemyCombinedPool(GetDisplayCapacityTotal());
    }
    else
    {
        // Display order is the reverse of the damage-consumption order.
        AddFriendlyPool(EAscHealthPoolType::Thorn, GetFriendlyPoolCapacity(EAscHealthPoolType::Thorn));
        AddFriendlyPool(EAscHealthPoolType::BasicHealth, GetFriendlyPoolCapacity(EAscHealthPoolType::BasicHealth));
        AddFriendlyPool(EAscHealthPoolType::Armor, GetFriendlyPoolCapacity(EAscHealthPoolType::Armor));
        AddFriendlyPool(EAscHealthPoolType::OverHealth, GetFriendlyPoolCapacity(EAscHealthPoolType::OverHealth));
    }

    CachedLayoutMode = VisualMode;
    CachedThornCapacity = GetFriendlyPoolCapacity(EAscHealthPoolType::Thorn);
    CachedBasicCapacity = GetFriendlyPoolCapacity(EAscHealthPoolType::BasicHealth);
    CachedArmorCapacity = GetFriendlyPoolCapacity(EAscHealthPoolType::Armor);
    CachedOverHealthCapacity = GetFriendlyPoolCapacity(EAscHealthPoolType::OverHealth);
    CachedEnemyTotalCapacity = GetDisplayCapacityTotal();
    CachedConfiguredWidth = GetRenderedHealthBarWidth();

    if (RuntimeSegments.Num() > 0)
    {
        CachedBarWidth = GetRenderedHealthBarWidth();
    }
}

void UAscSegmentedHealthBarWidget::RefreshSegments()
{
    const bool bEnemyStyle = VisualMode == EAscSegmentedHealthBarVisualMode::EnemyFlatRed;

    for (FAscHealthSegmentRuntime& Runtime : RuntimeSegments)
    {
        UAscHealthSegmentWidget* SegmentWidget = Runtime.Widget.Get();
        if (!SegmentWidget)
        {
            continue;
        }

        const float PoolCurrent = GetPoolCurrentValue(Runtime.PoolType);
        const float CellCurrent = FMath::Clamp(
            PoolCurrent - Runtime.PoolStartValue,
            0.0f,
            Runtime.CellCapacity);

        const float FillPercent = Runtime.CellCapacity > KINDA_SMALL_NUMBER
            ? CellCurrent / Runtime.CellCapacity
            : 0.0f;

        SegmentWidget->SetSegmentVisual(
            Runtime.PoolType,
            FillPercent,
            ResolveFillColor(Runtime.PoolType),
            bEnemyStyle);
    }

    const float DisplayCapacity = GetDisplayCapacityTotal();
    DisplayedHealthPercent = DisplayCapacity > KINDA_SMALL_NUMBER
        ? FMath::Clamp(CurrentSnapshot.GetCurrentTotal() / DisplayCapacity, 0.0f, 1.0f)
        : 0.0f;
}

void UAscSegmentedHealthBarWidget::AddFriendlyPool(
    EAscHealthPoolType PoolType,
    float Capacity)
{
    AddPoolSegments(PoolType, Capacity);
}

void UAscSegmentedHealthBarWidget::AddEnemyCombinedPool(float Capacity)
{
    // BasicHealth is only a runtime key here. EnemyFlatRed resolves every segment to red
    // and GetPoolCurrentValue returns the combined current health.
    AddPoolSegments(EAscHealthPoolType::BasicHealth, Capacity);
}

void UAscSegmentedHealthBarWidget::AddPoolSegments(
    EAscHealthPoolType PoolType,
    float Capacity)
{
    const int32 SegmentCount = GetSegmentCount(Capacity);
    if (SegmentCount <= 0 || !HB_Segments || !WidgetTree)
    {
        return;
    }

    const bool bHasPreviousPool = RuntimeSegments.Num() > 0;

    for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
    {
        const float PoolStartValue = SegmentIndex * ActiveStyle.HealthPerSegment;
        const float CellCapacity = FMath::Min(
            ActiveStyle.HealthPerSegment,
            FMath::Max(0.0f, Capacity - PoolStartValue));

        if (CellCapacity <= KINDA_SMALL_NUMBER)
        {
            continue;
        }

        USizeBox* CellSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        if (!CellSizeBox)
        {
            continue;
        }

        CellSizeBox->SetWidthOverride(ResolvedSegmentWidth);
        CellSizeBox->SetHeightOverride(ActiveStyle.SegmentHeight * GetPresentationScale());

        UAscHealthSegmentWidget* SegmentWidget =
            CreateWidget<UAscHealthSegmentWidget>(this, ActiveStyle.SegmentWidgetClass);

        if (!SegmentWidget)
        {
            continue;
        }

        CellSizeBox->SetContent(SegmentWidget);

        const bool bFirstCellOfPool = SegmentIndex == 0;
        float LeftPadding = 0.0f;

        if (RuntimeSegments.Num() > 0)
        {
            LeftPadding = bFirstCellOfPool && bHasPreviousPool
                ? ResolvedPoolGap
                : ResolvedSegmentGap;
        }

        if (UHorizontalBoxSlot* SegmentSlot = HB_Segments->AddChildToHorizontalBox(CellSizeBox))
        {
            SegmentSlot->SetPadding(FMargin(LeftPadding, 0.0f, 0.0f, 0.0f));
            SegmentSlot->SetHorizontalAlignment(HAlign_Left);
            SegmentSlot->SetVerticalAlignment(VAlign_Center);
        }

        CachedBarWidth += LeftPadding;

        FAscHealthSegmentRuntime& Runtime = RuntimeSegments.AddDefaulted_GetRef();
        Runtime.Widget = SegmentWidget;
        Runtime.PoolType = PoolType;
        Runtime.PoolStartValue = PoolStartValue;
        Runtime.CellCapacity = CellCapacity;
        Runtime.VisualStartX = CachedBarWidth;

        CachedBarWidth += ResolvedSegmentWidth;
    }
}

int32 UAscSegmentedHealthBarWidget::GetSegmentCount(float Capacity) const
{
    if (Capacity <= KINDA_SMALL_NUMBER || ActiveStyle.HealthPerSegment <= KINDA_SMALL_NUMBER)
    {
        return 0;
    }

    return FMath::CeilToInt(Capacity / ActiveStyle.HealthPerSegment);
}

int32 UAscSegmentedHealthBarWidget::GetTotalSegmentCountForCurrentLayout() const
{
    if (VisualMode == EAscSegmentedHealthBarVisualMode::EnemyFlatRed)
    {
        return GetSegmentCount(GetDisplayCapacityTotal());
    }

    return
        GetSegmentCount(GetFriendlyPoolCapacity(EAscHealthPoolType::Thorn)) +
        GetSegmentCount(GetFriendlyPoolCapacity(EAscHealthPoolType::BasicHealth)) +
        GetSegmentCount(GetFriendlyPoolCapacity(EAscHealthPoolType::Armor)) +
        GetSegmentCount(GetFriendlyPoolCapacity(EAscHealthPoolType::OverHealth));
}

int32 UAscSegmentedHealthBarWidget::GetFriendlyNonEmptyPoolCount() const
{
    int32 PoolCount = 0;

    PoolCount += GetSegmentCount(GetFriendlyPoolCapacity(EAscHealthPoolType::Thorn)) > 0 ? 1 : 0;
    PoolCount += GetSegmentCount(GetFriendlyPoolCapacity(EAscHealthPoolType::BasicHealth)) > 0 ? 1 : 0;
    PoolCount += GetSegmentCount(GetFriendlyPoolCapacity(EAscHealthPoolType::Armor)) > 0 ? 1 : 0;
    PoolCount += GetSegmentCount(GetFriendlyPoolCapacity(EAscHealthPoolType::OverHealth)) > 0 ? 1 : 0;

    return PoolCount;
}

void UAscSegmentedHealthBarWidget::CalculateResolvedLayoutMetrics()
{
    ResolvedSegmentWidth = 1.0f;
    ResolvedSegmentGap = FMath::Max(0.0f, ActiveStyle.SegmentGap) * GetPresentationScale();
    ResolvedPoolGap = FMath::Max(0.0f, ActiveStyle.PoolGap) * GetPresentationScale();

    const int32 TotalSegments = GetTotalSegmentCountForCurrentLayout();
    if (TotalSegments <= 0)
    {
        return;
    }

    const int32 PoolCount = VisualMode == EAscSegmentedHealthBarVisualMode::EnemyFlatRed
        ? 1
        : GetFriendlyNonEmptyPoolCount();

    const int32 SegmentGapCount = FMath::Max(0, TotalSegments - FMath::Max(1, PoolCount));
    const int32 PoolGapCount = FMath::Max(0, PoolCount - 1);
    const float TargetWidth = GetRenderedHealthBarWidth();

    const float PreferredGapWidth =
        SegmentGapCount * ResolvedSegmentGap +
        PoolGapCount * ResolvedPoolGap;

    float AvailableSegmentWidth = TargetWidth - PreferredGapWidth;
    float WidthPerSegment = AvailableSegmentWidth / static_cast<float>(TotalSegments);

    if (WidthPerSegment >= FMath::Max(0.01f, ActiveStyle.MinSegmentWidth * GetPresentationScale()))
    {
        // The total bar always occupies HealthBarWidth. Fewer cells become wider;
        // more cells become narrower while the configured gaps are preserved.
        ResolvedSegmentWidth = WidthPerSegment;
        return;
    }

    // Cells reached the preferred minimum. Collapse both gap types proportionally.
    const float MaximumPossibleCellWidth = TargetWidth / static_cast<float>(TotalSegments);
    ResolvedSegmentWidth = FMath::Min(
        FMath::Max(0.01f, ActiveStyle.MinSegmentWidth * GetPresentationScale()),
        MaximumPossibleCellWidth);

    const float RemainingGapBudget = FMath::Max(
        0.0f,
        TargetWidth - TotalSegments * ResolvedSegmentWidth);

    if (PreferredGapWidth > KINDA_SMALL_NUMBER)
    {
        const float GapScale = FMath::Clamp(
            RemainingGapBudget / PreferredGapWidth,
            0.0f,
            1.0f);

        ResolvedSegmentGap *= GapScale;
        ResolvedPoolGap *= GapScale;
    }
    else
    {
        ResolvedSegmentGap = 0.0f;
        ResolvedPoolGap = 0.0f;
    }

    const float FinalWidth =
        TotalSegments * ResolvedSegmentWidth +
        SegmentGapCount * ResolvedSegmentGap +
        PoolGapCount * ResolvedPoolGap;

    if (FinalWidth > TargetWidth + KINDA_SMALL_NUMBER)
    {
        ResolvedSegmentGap = 0.0f;
        ResolvedPoolGap = 0.0f;
        ResolvedSegmentWidth = FMath::Max(
            KINDA_SMALL_NUMBER,
            TargetWidth / static_cast<float>(TotalSegments));
    }
}

float UAscSegmentedHealthBarWidget::GetPoolCurrentValue(EAscHealthPoolType PoolType) const
{
    if (VisualMode == EAscSegmentedHealthBarVisualMode::EnemyFlatRed)
    {
        return CurrentSnapshot.GetCurrentTotal();
    }

    switch (PoolType)
    {
    case EAscHealthPoolType::Thorn:
        return FMath::Max(0.0f, CurrentSnapshot.Thorn);

    case EAscHealthPoolType::BasicHealth:
        return FMath::Max(0.0f, CurrentSnapshot.BasicHealth);

    case EAscHealthPoolType::Armor:
        return FMath::Max(0.0f, CurrentSnapshot.Armor);

    case EAscHealthPoolType::OverHealth:
        return FMath::Max(0.0f, CurrentSnapshot.OverHealth);

    default:
        return 0.0f;
    }
}


float UAscSegmentedHealthBarWidget::GetDisplayCapacityTotal() const
{
    return
        GetFriendlyPoolCapacity(EAscHealthPoolType::Thorn) +
        GetFriendlyPoolCapacity(EAscHealthPoolType::BasicHealth) +
        GetFriendlyPoolCapacity(EAscHealthPoolType::Armor) +
        GetFriendlyPoolCapacity(EAscHealthPoolType::OverHealth);
}

float UAscSegmentedHealthBarWidget::GetFriendlyPoolCapacity(EAscHealthPoolType PoolType) const
{
    switch (PoolType)
    {
    case EAscHealthPoolType::Thorn:
        return FMath::Max(0.0f, CurrentSnapshot.MaxThorn);

    case EAscHealthPoolType::BasicHealth:
        return FMath::Max(0.0f, CurrentSnapshot.MaxBasicHealth);

    case EAscHealthPoolType::Armor:
        return FMath::Max(0.0f, CurrentSnapshot.MaxArmor);

    case EAscHealthPoolType::OverHealth:
        return FMath::Max(0.0f, DynamicOverHealthCapacity);

    default:
        return 0.0f;
    }
}

FLinearColor UAscSegmentedHealthBarWidget::ResolveFillColor(EAscHealthPoolType PoolType) const
{
    if (VisualMode == EAscSegmentedHealthBarVisualMode::EnemyFlatRed)
    {
        return ActiveStyle.EnemyHealthColor;
    }

    switch (PoolType)
    {
    case EAscHealthPoolType::Thorn:
        return ActiveStyle.ThornColor;

    case EAscHealthPoolType::Armor:
        return ActiveStyle.ArmorColor;

    case EAscHealthPoolType::OverHealth:
        return ActiveStyle.OverHealthColor;

    case EAscHealthPoolType::BasicHealth:
    default:
        return ActiveStyle.BasicHealthColor;
    }
}

void UAscSegmentedHealthBarWidget::PlayDamagePulse()
{
    if (!ActiveStyle.bEnableDamagePulse || !Img_DamagePulse)
    {
        return;
    }

    ApplyEditorConfiguration();

    const float BoundaryX = CalculateCurrentBoundaryX();

    if (UCanvasPanelSlot* DamageSlot = Cast<UCanvasPanelSlot>(Img_DamagePulse->Slot))
    {
        DamageSlot->SetAutoSize(false);
        DamageSlot->SetAlignment(ActiveStyle.DamagePulsePivot);
        DamageSlot->SetAnchors(FAnchors(0.0f, 0.0f));
        DamageSlot->SetSize(FVector2D(
            FMath::Max(1.0f, ActiveStyle.DamagePulseDrawSize.X * GetPresentationScale()),
            FMath::Max(1.0f, ActiveStyle.DamagePulseDrawSize.Y * GetPresentationScale())));
        DamageSlot->SetPosition(FVector2D(
            BoundaryX + ActiveStyle.DamagePulsePositionOffset.X * GetPresentationScale(),
            GetRenderedHealthBarHeight() * 0.5f + ActiveStyle.DamagePulsePositionOffset.Y * GetPresentationScale()));
    }

    Img_DamagePulse->SetColorAndOpacity(ActiveStyle.DamagePulseColor);
    Img_DamagePulse->SetRenderTransformPivot(ActiveStyle.DamagePulsePivot);
    Img_DamagePulse->SetRenderTransformAngle(ActiveStyle.DamagePulseRenderAngle);
    Img_DamagePulse->SetRenderOpacity(FMath::Clamp(ActiveStyle.DamagePulseStartOpacity, 0.0f, 1.0f));
    Img_DamagePulse->SetRenderScale(ActiveStyle.DamagePulseStartScale);
    Img_DamagePulse->SetVisibility(ESlateVisibility::HitTestInvisible);

    DamagePulseElapsed = 0.0f;
    bDamagePulseActive = true;

    BP_OnDamagePulseStarted(BoundaryX);
}

void UAscSegmentedHealthBarWidget::StopDamagePulse()
{
    bDamagePulseActive = false;
    DamagePulseElapsed = 0.0f;

    if (Img_DamagePulse)
    {
        Img_DamagePulse->SetRenderOpacity(0.0f);
        Img_DamagePulse->SetRenderScale(ActiveStyle.DamagePulseStartScale);
        Img_DamagePulse->SetVisibility(ESlateVisibility::Collapsed);
    }
}

float UAscSegmentedHealthBarWidget::CalculateCurrentBoundaryX() const
{
    float BoundaryX = 0.0f;

    for (const FAscHealthSegmentRuntime& Runtime : RuntimeSegments)
    {
        const float PoolCurrent = GetPoolCurrentValue(Runtime.PoolType);
        const float CellCurrent = FMath::Clamp(
            PoolCurrent - Runtime.PoolStartValue,
            0.0f,
            Runtime.CellCapacity);

        if (CellCurrent <= KINDA_SMALL_NUMBER)
        {
            continue;
        }

        const float FillPercent = Runtime.CellCapacity > KINDA_SMALL_NUMBER
            ? CellCurrent / Runtime.CellCapacity
            : 0.0f;

        BoundaryX = Runtime.VisualStartX + ResolvedSegmentWidth * FillPercent;
    }

    return FMath::Clamp(BoundaryX, 0.0f, GetRenderedHealthBarWidth());
}

void UAscSegmentedHealthBarWidget::UpdateDamagePulseVisual(float NormalizedTime)
{
    if (!Img_DamagePulse)
    {
        return;
    }

    const float ClampedTime = FMath::Clamp(NormalizedTime, 0.0f, 1.0f);
    const float ScaleAlpha = FMath::Pow(
        ClampedTime,
        FMath::Max(0.01f, ActiveStyle.DamagePulseScaleExponent));
    const float FadeAlpha = FMath::Pow(
        ClampedTime,
        FMath::Max(0.01f, ActiveStyle.DamagePulseFadeExponent));

    Img_DamagePulse->SetRenderScale(FMath::Lerp(
        ActiveStyle.DamagePulseStartScale,
        ActiveStyle.DamagePulseEndScale,
        ScaleAlpha));

    Img_DamagePulse->SetRenderOpacity(FMath::Lerp(
        FMath::Clamp(ActiveStyle.DamagePulseStartOpacity, 0.0f, 1.0f),
        FMath::Clamp(ActiveStyle.DamagePulseEndOpacity, 0.0f, 1.0f),
        FadeAlpha));
}

