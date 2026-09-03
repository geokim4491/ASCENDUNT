#pragma once

#include "CoreMinimal.h"
#include "AscSettingsPageWidgetBase.h"
#include "AscSettingsControlsPageWidget.generated.h"

class UAscSettingSliderWidget;
class UAscSettingToggleWidget;

UCLASS()
class ASCENDUNT_API UAscSettingsControlsPageWidget : public UAscSettingsPageWidgetBase
{
    GENERATED_BODY()

public:
    virtual void RefreshFromSettings() override;

protected:
    virtual void NativeConstruct() override;

    UFUNCTION()
    void HandleMouseSensitivityChanged(float Value);

    UFUNCTION()
    void HandleADSSensitivityChanged(float Value);

    UFUNCTION()
    void HandleInvertYChanged(bool bEnabled);

protected:
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UAscSettingSliderWidget> Setting_MouseSensitivity;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UAscSettingSliderWidget> Setting_ADSSensitivity;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UAscSettingToggleWidget> Setting_InvertY;
};
