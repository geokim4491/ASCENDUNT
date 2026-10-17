#pragma once

#include "CoreMinimal.h"
#include "AscSettingsPageWidgetBase.h"
#include "AscSettingsVideoPageWidget.generated.h"

class UAscSettingSelectorWidget;
class UAscSettingSliderWidget;
class UAscSettingToggleWidget;

UCLASS()
class ASCENDUNT_API UAscSettingsVideoPageWidget : public UAscSettingsPageWidgetBase
{
    GENERATED_BODY()

public:
    virtual void RefreshFromSettings() override;

protected:
    virtual void NativeConstruct() override;

    UFUNCTION() void HandleDisplayModeChanged(int32 Index);
    UFUNCTION() void HandleResolutionChanged(int32 Index);
    UFUNCTION() void HandleVSyncChanged(bool bEnabled);
    UFUNCTION() void HandleFrameLimitChanged(int32 Index);
    UFUNCTION() void HandleBrightnessChanged(float Value);

    void RebuildResolutionOptions();

protected:
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UAscSettingSelectorWidget> Setting_DisplayMode;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UAscSettingSelectorWidget> Setting_Resolution;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UAscSettingToggleWidget> Setting_VSync;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UAscSettingSelectorWidget> Setting_FrameLimit;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UAscSettingSliderWidget> Setting_Brightness;

private:
    TArray<FIntPoint> ResolutionOptions;
    TArray<float> FrameLimitOptions = { 60.0f, 120.0f, 144.0f, 165.0f, 240.0f, 0.0f };
};
