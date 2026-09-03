#pragma once

#include "CoreMinimal.h"
#include "AscSettingsPageWidgetBase.h"
#include "AscSettingsGraphicsPageWidget.generated.h"

class UAscSettingSelectorWidget;
class UAscSettingToggleWidget;

UCLASS()
class ASCENDUNT_API UAscSettingsGraphicsPageWidget : public UAscSettingsPageWidgetBase
{
    GENERATED_BODY()

public:
    virtual void RefreshFromSettings() override;

protected:
    virtual void NativeConstruct() override;

    UFUNCTION() void HandleOverallQualityChanged(int32 Index);
    UFUNCTION() void HandleAAMethodChanged(int32 Index);
    UFUNCTION() void HandleAAQualityChanged(int32 Index);
    UFUNCTION() void HandleViewDistanceChanged(int32 Index);
    UFUNCTION() void HandleShadowChanged(int32 Index);
    UFUNCTION() void HandleTextureChanged(int32 Index);
    UFUNCTION() void HandleEffectsChanged(int32 Index);
    UFUNCTION() void HandlePostProcessChanged(int32 Index);
    UFUNCTION() void HandleMotionBlurChanged(bool bEnabled);

    void ApplyNonResolutionAndRefreshPreset();
    TArray<FText> MakeQualityOptions(bool bIncludeCustom) const;

protected:
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UAscSettingSelectorWidget> Setting_OverallQuality;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UAscSettingSelectorWidget> Setting_AAMethod;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UAscSettingSelectorWidget> Setting_AAQuality;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UAscSettingSelectorWidget> Setting_ViewDistance;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UAscSettingSelectorWidget> Setting_Shadow;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UAscSettingSelectorWidget> Setting_Texture;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UAscSettingSelectorWidget> Setting_Effects;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UAscSettingSelectorWidget> Setting_PostProcess;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UAscSettingToggleWidget> Setting_MotionBlur;
};
