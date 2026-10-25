#include "AscHealthSegmentWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Brushes/SlateColorBrush.h"

void UAscHealthSegmentWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyFlatNoOutlineStyle();
}

void UAscHealthSegmentWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    EnsureNativeFallbackWidget();
    ApplyFlatNoOutlineStyle();
}

void UAscHealthSegmentWidget::SetSegmentVisual(
    EAscHealthPoolType InPoolType,
    float InFillPercent,
    const FLinearColor& InFillColor,
    bool bInEnemyStyle)
{
    EnsureNativeFallbackWidget();
    ApplyFlatNoOutlineStyle();

    const float ClampedPercent = FMath::Clamp(InFillPercent, 0.0f, 1.0f);

    if (Img_Background)
    {
        // Only tint the supplied background image. Its brush/texture remains owned by the WBP.
        Img_Background->SetColorAndOpacity(
            bInEnemyStyle ? EnemyBackgroundTint : FriendlyBackgroundTint);
    }

    if (PB_Fill)
    {
        PB_Fill->SetPercent(ClampedPercent);
        PB_Fill->SetFillColorAndOpacity(InFillColor);
    }

    BP_OnSegmentVisualChanged(InPoolType, ClampedPercent, InFillColor, bInEnemyStyle);
}

void UAscHealthSegmentWidget::ApplyFlatNoOutlineStyle()
{
    if (Img_Background)
    {
        // Ignore any border/outlined texture configured in the WBP. A copied color brush
        // renders a plain rectangle and still receives the friendly/enemy tint below.
        const FSlateColorBrush FlatBackgroundBrush(FLinearColor::White);
        Img_Background->SetBrush(FlatBackgroundBrush);
    }

    if (PB_Fill)
    {
        // UProgressBar's default style contains its own background/border imagery.
        // The separate Img_Background owns the empty-cell color, so make the bar itself fill-only.
        FProgressBarStyle FlatStyle = PB_Fill->GetWidgetStyle();
        FlatStyle.BackgroundImage = FSlateColorBrush(FLinearColor::Transparent);
        FlatStyle.FillImage = FSlateColorBrush(FLinearColor::White);
        FlatStyle.MarqueeImage = FSlateColorBrush(FLinearColor::Transparent);
        PB_Fill->SetWidgetStyle(FlatStyle);
        PB_Fill->SetBorderPadding(FVector2D::ZeroVector);
    }
}

void UAscHealthSegmentWidget::EnsureNativeFallbackWidget()
{
    if ((Img_Background && PB_Fill) || !WidgetTree || WidgetTree->RootWidget)
    {
        return;
    }

    UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(
        UOverlay::StaticClass(), TEXT("SegmentRoot"));

    Img_Background = WidgetTree->ConstructWidget<UImage>(
        UImage::StaticClass(), TEXT("Img_Background"));
    Img_Background->SetColorAndOpacity(FriendlyBackgroundTint);

    if (UOverlaySlot* BackgroundSlot = RootOverlay->AddChildToOverlay(Img_Background))
    {
        BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
        BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
    }

    PB_Fill = WidgetTree->ConstructWidget<UProgressBar>(
        UProgressBar::StaticClass(), TEXT("PB_Fill"));
    PB_Fill->SetBarFillType(EProgressBarFillType::LeftToRight);
    PB_Fill->SetPercent(0.0f);

    if (UOverlaySlot* FillSlot = RootOverlay->AddChildToOverlay(PB_Fill))
    {
        FillSlot->SetHorizontalAlignment(HAlign_Fill);
        FillSlot->SetVerticalAlignment(VAlign_Fill);
    }

    WidgetTree->RootWidget = RootOverlay;
}
