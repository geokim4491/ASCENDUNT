#include "AscDownedIndicatorWidget.h"

#include "Components/TextBlock.h"

void UAscDownedIndicatorWidget::SetBleedoutRemainingTime(float RemainingSeconds)
{
    if (!Txt_DownedRemaining)
    {
        return;
    }

    const float Clamped = FMath::Max(0.0f, RemainingSeconds);
    Txt_DownedRemaining->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), Clamped)));
}
