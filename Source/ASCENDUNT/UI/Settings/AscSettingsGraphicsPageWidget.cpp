#include "AscSettingsGraphicsPageWidget.h"

#include "AscSettingSelectorWidget.h"
#include "AscSettingToggleWidget.h"
#include "ASCENDUNT/System/AscGameUserSettings.h"

#define LOCTEXT_NAMESPACE "AscSettingsGraphicsPage"

void UAscSettingsGraphicsPageWidget::NativeConstruct()
{
    Super::NativeConstruct();

#define BIND_SELECTOR(Widget, Handler) \
    if (Widget) { Widget->OnSelectionChanged.RemoveDynamic(this, &ThisClass::Handler); Widget->OnSelectionChanged.AddDynamic(this, &ThisClass::Handler); }

    BIND_SELECTOR(Setting_OverallQuality, HandleOverallQualityChanged);
    BIND_SELECTOR(Setting_AAMethod, HandleAAMethodChanged);
    BIND_SELECTOR(Setting_AAQuality, HandleAAQualityChanged);
    BIND_SELECTOR(Setting_ViewDistance, HandleViewDistanceChanged);
    BIND_SELECTOR(Setting_Shadow, HandleShadowChanged);
    BIND_SELECTOR(Setting_Texture, HandleTextureChanged);
    BIND_SELECTOR(Setting_Effects, HandleEffectsChanged);
    BIND_SELECTOR(Setting_PostProcess, HandlePostProcessChanged);
#undef BIND_SELECTOR

    if (Setting_MotionBlur)
    {
        Setting_MotionBlur->OnToggleChanged.RemoveDynamic(this, &ThisClass::HandleMotionBlurChanged);
        Setting_MotionBlur->OnToggleChanged.AddDynamic(this, &ThisClass::HandleMotionBlurChanged);
    }

    RefreshFromSettings();
}

TArray<FText> UAscSettingsGraphicsPageWidget::MakeQualityOptions(bool bIncludeCustom) const
{
    TArray<FText> Result = {
        LOCTEXT("Low", "LOW"),
        LOCTEXT("Medium", "MEDIUM"),
        LOCTEXT("High", "HIGH"),
        LOCTEXT("Epic", "EPIC")
    };

    if (bIncludeCustom)
    {
        Result.Add(LOCTEXT("Custom", "CUSTOM"));
    }
    return Result;
}

void UAscSettingsGraphicsPageWidget::RefreshFromSettings()
{
    const UAscGameUserSettings* Settings = UAscGameUserSettings::GetAscGameUserSettings();
    if (!Settings)
    {
        return;
    }

    const TArray<FText> QualityOptions = MakeQualityOptions(false);
    const TArray<FText> OverallOptions = MakeQualityOptions(true);

    if (Setting_OverallQuality)
    {
        const int32 Overall = Settings->GetOverallScalabilityLevel();
        Setting_OverallQuality->ConfigureSelector(
            LOCTEXT("GraphicsQuality", "GRAPHICS QUALITY"),
            LOCTEXT("GraphicsQualityDesc", "Change all scalability settings at once."),
            OverallOptions,
            (Overall >= 0 && Overall <= 3) ? Overall : 4);
    }

    if (Setting_AAMethod)
    {
        const TArray<FText> AAOptions = { LOCTEXT("FXAA", "FXAA"), LOCTEXT("TAA", "TAA"), LOCTEXT("TSR", "TSR") };
        int32 AAIndex = 2;
        switch (Settings->GetAntiAliasingMethod())
        {
        case EAscAntiAliasingMethod::FXAA: AAIndex = 0; break;
        case EAscAntiAliasingMethod::TAA: AAIndex = 1; break;
        case EAscAntiAliasingMethod::TSR: default: AAIndex = 2; break;
        }
        Setting_AAMethod->ConfigureSelector(
            LOCTEXT("AAMethod", "ANTI-ALIASING METHOD"),
            LOCTEXT("AAMethodDesc", "Select the anti-aliasing technique."),
            AAOptions,
            AAIndex);
    }

#define CONFIG_QUALITY(Widget, NameKey, NameText, DescKey, DescText, Getter) \
    if (Widget) { Widget->ConfigureSelector(LOCTEXT(NameKey, NameText), LOCTEXT(DescKey, DescText), QualityOptions, FMath::Clamp(Settings->Getter(), 0, 3)); }

    CONFIG_QUALITY(Setting_AAQuality, "AAQuality", "ANTI-ALIASING QUALITY", "AAQualityDesc", "Adjust anti-aliasing quality.", GetAntiAliasingQuality);
    CONFIG_QUALITY(Setting_ViewDistance, "ViewDistance", "VIEW DISTANCE", "ViewDistanceDesc", "Adjust how far objects remain visible.", GetViewDistanceQuality);
    CONFIG_QUALITY(Setting_Shadow, "ShadowQuality", "SHADOW QUALITY", "ShadowQualityDesc", "Adjust shadow rendering quality.", GetShadowQuality);
    CONFIG_QUALITY(Setting_Texture, "TextureQuality", "TEXTURE QUALITY", "TextureQualityDesc", "Adjust texture quality.", GetTextureQuality);
    CONFIG_QUALITY(Setting_Effects, "EffectsQuality", "EFFECTS QUALITY", "EffectsQualityDesc", "Adjust effects quality.", GetVisualEffectQuality);
    CONFIG_QUALITY(Setting_PostProcess, "PostProcessQuality", "POST PROCESSING", "PostProcessQualityDesc", "Adjust post-processing quality.", GetPostProcessingQuality);
#undef CONFIG_QUALITY

    if (Setting_MotionBlur)
    {
        Setting_MotionBlur->ConfigureToggle(
            LOCTEXT("MotionBlur", "MOTION BLUR"),
            LOCTEXT("MotionBlurDesc", "Enable or disable motion blur."),
            Settings->IsMotionBlurEnabled());
    }
}

void UAscSettingsGraphicsPageWidget::ApplyNonResolutionAndRefreshPreset()
{
    if (UAscGameUserSettings* Settings = UAscGameUserSettings::GetAscGameUserSettings())
    {
        Settings->ApplyNonResolutionSettings();

        if (Setting_OverallQuality)
        {
            const int32 Overall = Settings->GetOverallScalabilityLevel();
            Setting_OverallQuality->SetSelectedIndex((Overall >= 0 && Overall <= 3) ? Overall : 4, false);
        }
    }
}

void UAscSettingsGraphicsPageWidget::HandleOverallQualityChanged(int32 Index)
{
    if (Index < 0 || Index > 3)
    {
        return;
    }

    if (UAscGameUserSettings* Settings = UAscGameUserSettings::GetAscGameUserSettings())
    {
        Settings->SetOverallScalabilityLevel(Index);
        Settings->ApplyNonResolutionSettings();
        RefreshFromSettings();
    }
}

void UAscSettingsGraphicsPageWidget::HandleAAMethodChanged(int32 Index)
{
    if (UAscGameUserSettings* Settings = UAscGameUserSettings::GetAscGameUserSettings())
    {
        const EAscAntiAliasingMethod Method = Index == 0 ? EAscAntiAliasingMethod::FXAA : (Index == 1 ? EAscAntiAliasingMethod::TAA : EAscAntiAliasingMethod::TSR);
        Settings->SetAntiAliasingMethod(Method);
        ApplyNonResolutionAndRefreshPreset();
    }
}

void UAscSettingsGraphicsPageWidget::HandleAAQualityChanged(int32 Index) { if (UAscGameUserSettings* S = UAscGameUserSettings::GetAscGameUserSettings()) { S->SetAntiAliasingQuality(Index); ApplyNonResolutionAndRefreshPreset(); } }
void UAscSettingsGraphicsPageWidget::HandleViewDistanceChanged(int32 Index) { if (UAscGameUserSettings* S = UAscGameUserSettings::GetAscGameUserSettings()) { S->SetViewDistanceQuality(Index); ApplyNonResolutionAndRefreshPreset(); } }
void UAscSettingsGraphicsPageWidget::HandleShadowChanged(int32 Index) { if (UAscGameUserSettings* S = UAscGameUserSettings::GetAscGameUserSettings()) { S->SetShadowQuality(Index); ApplyNonResolutionAndRefreshPreset(); } }
void UAscSettingsGraphicsPageWidget::HandleTextureChanged(int32 Index) { if (UAscGameUserSettings* S = UAscGameUserSettings::GetAscGameUserSettings()) { S->SetTextureQuality(Index); ApplyNonResolutionAndRefreshPreset(); } }
void UAscSettingsGraphicsPageWidget::HandleEffectsChanged(int32 Index) { if (UAscGameUserSettings* S = UAscGameUserSettings::GetAscGameUserSettings()) { S->SetVisualEffectQuality(Index); ApplyNonResolutionAndRefreshPreset(); } }
void UAscSettingsGraphicsPageWidget::HandlePostProcessChanged(int32 Index) { if (UAscGameUserSettings* S = UAscGameUserSettings::GetAscGameUserSettings()) { S->SetPostProcessingQuality(Index); ApplyNonResolutionAndRefreshPreset(); } }
void UAscSettingsGraphicsPageWidget::HandleMotionBlurChanged(bool bEnabled) { if (UAscGameUserSettings* S = UAscGameUserSettings::GetAscGameUserSettings()) { S->SetMotionBlurEnabled(bEnabled); S->ApplyNonResolutionSettings(); } }

#undef LOCTEXT_NAMESPACE
