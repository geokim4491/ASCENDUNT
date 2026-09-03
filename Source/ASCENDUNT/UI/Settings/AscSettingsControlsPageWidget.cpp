#include "AscSettingsControlsPageWidget.h"

#include "AscSettingSliderWidget.h"
#include "AscSettingToggleWidget.h"
#include "ASCENDUNT/System/AscGameUserSettings.h"

#define LOCTEXT_NAMESPACE "AscSettingsControlsPage"

void UAscSettingsControlsPageWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Setting_MouseSensitivity)
    {
        Setting_MouseSensitivity->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleMouseSensitivityChanged);
        Setting_MouseSensitivity->OnValueChanged.AddDynamic(this, &ThisClass::HandleMouseSensitivityChanged);
    }
    if (Setting_ADSSensitivity)
    {
        Setting_ADSSensitivity->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleADSSensitivityChanged);
        Setting_ADSSensitivity->OnValueChanged.AddDynamic(this, &ThisClass::HandleADSSensitivityChanged);
    }
    if (Setting_InvertY)
    {
        Setting_InvertY->OnToggleChanged.RemoveDynamic(this, &ThisClass::HandleInvertYChanged);
        Setting_InvertY->OnToggleChanged.AddDynamic(this, &ThisClass::HandleInvertYChanged);
    }

    RefreshFromSettings();
}

void UAscSettingsControlsPageWidget::RefreshFromSettings()
{
    const UAscGameUserSettings* Settings = UAscGameUserSettings::GetAscGameUserSettings();
    if (!Settings)
    {
        return;
    }

    if (Setting_MouseSensitivity)
    {
        Setting_MouseSensitivity->ConfigureSlider(
            LOCTEXT("MouseSensitivity", "MOUSE SENSITIVITY"),
            LOCTEXT("MouseSensitivityDesc", "Adjust camera look sensitivity."),
            0.05f, 10.0f, 0.05f, 1.0f, 2, FText::GetEmpty(), Settings->GetMouseSensitivity());
    }

    if (Setting_ADSSensitivity)
    {
        Setting_ADSSensitivity->ConfigureSlider(
            LOCTEXT("ADSSensitivity", "ADS SENSITIVITY"),
            LOCTEXT("ADSSensitivityDesc", "Multiplier applied while aiming down sights."),
            0.05f, 2.0f, 0.05f, 100.0f, 0, FText::FromString(TEXT("%")), Settings->GetADSSensitivityMultiplier());
    }

    if (Setting_InvertY)
    {
        Setting_InvertY->ConfigureToggle(
            LOCTEXT("InvertY", "INVERT Y AXIS"),
            LOCTEXT("InvertYDesc", "Invert vertical camera input."),
            Settings->IsInvertYAxisEnabled());
    }
}

void UAscSettingsControlsPageWidget::HandleMouseSensitivityChanged(float Value)
{
    if (UAscGameUserSettings* Settings = UAscGameUserSettings::GetAscGameUserSettings())
    {
        Settings->SetMouseSensitivity(Value);
    }
}

void UAscSettingsControlsPageWidget::HandleADSSensitivityChanged(float Value)
{
    if (UAscGameUserSettings* Settings = UAscGameUserSettings::GetAscGameUserSettings())
    {
        Settings->SetADSSensitivityMultiplier(Value);
    }
}

void UAscSettingsControlsPageWidget::HandleInvertYChanged(bool bEnabled)
{
    if (UAscGameUserSettings* Settings = UAscGameUserSettings::GetAscGameUserSettings())
    {
        Settings->SetInvertYAxisEnabled(bEnabled);
    }
}

#undef LOCTEXT_NAMESPACE
