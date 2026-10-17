#include "AscSettingSliderWidget.h"

#include "Components/Slider.h"
#include "Components/TextBlock.h"

void UAscSettingSliderWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Slider_Value)
    {
        Slider_Value->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleSliderValueChanged);
        Slider_Value->OnValueChanged.AddDynamic(this, &ThisClass::HandleSliderValueChanged);
        Slider_Value->SetMinValue(MinValue);
        Slider_Value->SetMaxValue(MaxValue);
        Slider_Value->SetValue(CurrentValue);
    }

    RefreshValueText();
}

void UAscSettingSliderWidget::ConfigureSlider(
    const FText& InName,
    const FText& InDescription,
    float InMinValue,
    float InMaxValue,
    float InStepSize,
    float InDisplayMultiplier,
    int32 InDecimalPlaces,
    const FText& InSuffix,
    float InValue)
{
    MinValue = FMath::Min(InMinValue, InMaxValue);
    MaxValue = FMath::Max(InMinValue, InMaxValue);
    StepSize = FMath::Max(0.0f, InStepSize);
    DisplayMultiplier = InDisplayMultiplier;
    DecimalPlaces = FMath::Clamp(InDecimalPlaces, 0, 4);
    Suffix = InSuffix;

    if (Txt_Name)
    {
        Txt_Name->SetText(InName);
    }
    if (Txt_Description)
    {
        Txt_Description->SetText(InDescription);
    }

    if (Slider_Value)
    {
        Slider_Value->SetMinValue(MinValue);
        Slider_Value->SetMaxValue(MaxValue);
    }

    SetValue(InValue, false);
}

void UAscSettingSliderWidget::SetValue(float InValue, bool bBroadcast)
{
    const float NewValue = SnapValue(InValue);
    CurrentValue = NewValue;

    if (Slider_Value)
    {
        bUpdatingSliderInternally = true;
        Slider_Value->SetValue(CurrentValue);
        bUpdatingSliderInternally = false;
    }

    RefreshValueText();

    if (bBroadcast)
    {
        OnValueChanged.Broadcast(CurrentValue);
    }
}

void UAscSettingSliderWidget::HandleSliderValueChanged(float NewValue)
{
    if (bUpdatingSliderInternally)
    {
        return;
    }

    const float SnappedValue = SnapValue(NewValue);
    CurrentValue = SnappedValue;

    if (Slider_Value && !FMath::IsNearlyEqual(SnappedValue, NewValue))
    {
        bUpdatingSliderInternally = true;
        Slider_Value->SetValue(SnappedValue);
        bUpdatingSliderInternally = false;
    }

    RefreshValueText();
    OnValueChanged.Broadcast(CurrentValue);
}

float UAscSettingSliderWidget::SnapValue(float InValue) const
{
    const float ClampedValue = FMath::Clamp(InValue, MinValue, MaxValue);
    if (StepSize <= KINDA_SMALL_NUMBER)
    {
        return ClampedValue;
    }

    const float Steps = FMath::RoundToFloat((ClampedValue - MinValue) / StepSize);
    return FMath::Clamp(MinValue + Steps * StepSize, MinValue, MaxValue);
}

void UAscSettingSliderWidget::RefreshValueText()
{
    if (!Txt_Value)
    {
        return;
    }

    const float DisplayValue = CurrentValue * DisplayMultiplier;
    const FString ValueString = FString::Printf(TEXT("%.*f%s"), DecimalPlaces, DisplayValue, *Suffix.ToString());
    Txt_Value->SetText(FText::FromString(ValueString));
}
