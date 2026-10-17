#include "AscSettingsVideoPageWidget.h"

#include "AscSettingSelectorWidget.h"
#include "AscSettingSliderWidget.h"
#include "AscSettingToggleWidget.h"
#include "ASCENDUNT/System/AscGameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"

#define LOCTEXT_NAMESPACE "AscSettingsVideoPage"

void UAscSettingsVideoPageWidget::NativeConstruct()
{
    Super::NativeConstruct();

#define BIND_SELECTOR(Widget, Handler) \
    if (Widget) { Widget->OnSelectionChanged.RemoveDynamic(this, &ThisClass::Handler); Widget->OnSelectionChanged.AddDynamic(this, &ThisClass::Handler); }

    BIND_SELECTOR(Setting_DisplayMode, HandleDisplayModeChanged);
    BIND_SELECTOR(Setting_Resolution, HandleResolutionChanged);
    BIND_SELECTOR(Setting_FrameLimit, HandleFrameLimitChanged);
#undef BIND_SELECTOR

    if (Setting_VSync)
    {
        Setting_VSync->OnToggleChanged.RemoveDynamic(this, &ThisClass::HandleVSyncChanged);
        Setting_VSync->OnToggleChanged.AddDynamic(this, &ThisClass::HandleVSyncChanged);
    }
    if (Setting_Brightness)
    {
        Setting_Brightness->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleBrightnessChanged);
        Setting_Brightness->OnValueChanged.AddDynamic(this, &ThisClass::HandleBrightnessChanged);
    }

    RebuildResolutionOptions();
    RefreshFromSettings();
}

void UAscSettingsVideoPageWidget::RebuildResolutionOptions()
{
    ResolutionOptions.Reset();
    UKismetSystemLibrary::GetSupportedFullscreenResolutions(ResolutionOptions);

    ResolutionOptions.Sort([](const FIntPoint& A, const FIntPoint& B)
    {
        const int64 AreaA = static_cast<int64>(A.X) * A.Y;
        const int64 AreaB = static_cast<int64>(B.X) * B.Y;
        return AreaA == AreaB ? A.X < B.X : AreaA < AreaB;
    });

    for (int32 Index = ResolutionOptions.Num() - 1; Index > 0; --Index)
    {
        if (ResolutionOptions[Index] == ResolutionOptions[Index - 1])
        {
            ResolutionOptions.RemoveAt(Index);
        }
    }
}

void UAscSettingsVideoPageWidget::RefreshFromSettings()
{
    const UAscGameUserSettings* Settings = UAscGameUserSettings::GetAscGameUserSettings();
    if (!Settings)
    {
        return;
    }

    if (Setting_DisplayMode)
    {
        const TArray<FText> Modes = {
            LOCTEXT("Fullscreen", "FULLSCREEN"),
            LOCTEXT("Borderless", "BORDERLESS"),
            LOCTEXT("Windowed", "WINDOWED")
        };

        int32 ModeIndex = 1;
        switch (Settings->GetFullscreenMode())
        {
        case EWindowMode::Fullscreen: ModeIndex = 0; break;
        case EWindowMode::WindowedFullscreen: ModeIndex = 1; break;
        case EWindowMode::Windowed: default: ModeIndex = 2; break;
        }

        Setting_DisplayMode->ConfigureSelector(
            LOCTEXT("DisplayMode", "DISPLAY MODE"),
            LOCTEXT("DisplayModeDesc", "Choose fullscreen, borderless, or windowed display."),
            Modes,
            ModeIndex);
    }

    if (Setting_Resolution)
    {
        const FIntPoint CurrentResolution = Settings->GetScreenResolution();
        if (!ResolutionOptions.Contains(CurrentResolution))
        {
            ResolutionOptions.Add(CurrentResolution);
        }

        TArray<FText> ResolutionTexts;
        ResolutionTexts.Reserve(ResolutionOptions.Num());
        int32 SelectedIndex = 0;
        for (int32 Index = 0; Index < ResolutionOptions.Num(); ++Index)
        {
            const FIntPoint& Resolution = ResolutionOptions[Index];
            ResolutionTexts.Add(FText::FromString(FString::Printf(TEXT("%d x %d"), Resolution.X, Resolution.Y)));
            if (Resolution == CurrentResolution)
            {
                SelectedIndex = Index;
            }
        }

        Setting_Resolution->ConfigureSelector(
            LOCTEXT("Resolution", "RESOLUTION"),
            LOCTEXT("ResolutionDesc", "Choose the output resolution. Applied when APPLY is pressed."),
            ResolutionTexts,
            SelectedIndex);
    }

    if (Setting_VSync)
    {
        Setting_VSync->ConfigureToggle(
            LOCTEXT("VSync", "V-SYNC"),
            LOCTEXT("VSyncDesc", "Synchronize frame presentation with the display refresh."),
            Settings->IsVSyncEnabled());
    }

    if (Setting_FrameLimit)
    {
        const TArray<FText> FrameTexts = {
            FText::FromString(TEXT("60 FPS")), FText::FromString(TEXT("120 FPS")),
            FText::FromString(TEXT("144 FPS")), FText::FromString(TEXT("165 FPS")),
            FText::FromString(TEXT("240 FPS")), LOCTEXT("Unlimited", "UNLIMITED")
        };

        const float CurrentLimit = Settings->GetFrameRateLimit();
        int32 SelectedIndex = FrameLimitOptions.Num() - 1;
        if (CurrentLimit > 0.0f)
        {
            float BestDistance = TNumericLimits<float>::Max();
            for (int32 Index = 0; Index < FrameLimitOptions.Num() - 1; ++Index)
            {
                const float Distance = FMath::Abs(FrameLimitOptions[Index] - CurrentLimit);
                if (Distance < BestDistance)
                {
                    BestDistance = Distance;
                    SelectedIndex = Index;
                }
            }
        }

        Setting_FrameLimit->ConfigureSelector(
            LOCTEXT("FrameLimit", "FRAME RATE LIMIT"),
            LOCTEXT("FrameLimitDesc", "Set the maximum client frame rate."),
            FrameTexts,
            SelectedIndex);
    }

    if (Setting_Brightness)
    {
        Setting_Brightness->ConfigureSlider(
            LOCTEXT("Brightness", "BRIGHTNESS"),
            LOCTEXT("BrightnessDesc", "Adjust display gamma."),
            1.6f, 3.0f, 0.05f, 1.0f, 2, FText::GetEmpty(), Settings->GetBrightnessGamma());
    }
}

void UAscSettingsVideoPageWidget::HandleDisplayModeChanged(int32 Index)
{
    if (UAscGameUserSettings* Settings = UAscGameUserSettings::GetAscGameUserSettings())
    {
        const EWindowMode::Type Mode = Index == 0 ? EWindowMode::Fullscreen : (Index == 1 ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed);
        Settings->SetFullscreenMode(Mode);
    }
}

void UAscSettingsVideoPageWidget::HandleResolutionChanged(int32 Index)
{
    if (!ResolutionOptions.IsValidIndex(Index))
    {
        return;
    }

    if (UAscGameUserSettings* Settings = UAscGameUserSettings::GetAscGameUserSettings())
    {
        Settings->SetScreenResolution(ResolutionOptions[Index]);
    }
}

void UAscSettingsVideoPageWidget::HandleVSyncChanged(bool bEnabled)
{
    if (UAscGameUserSettings* Settings = UAscGameUserSettings::GetAscGameUserSettings())
    {
        Settings->SetVSyncEnabled(bEnabled);
        Settings->ApplyNonResolutionSettings();
    }
}

void UAscSettingsVideoPageWidget::HandleFrameLimitChanged(int32 Index)
{
    if (!FrameLimitOptions.IsValidIndex(Index))
    {
        return;
    }

    if (UAscGameUserSettings* Settings = UAscGameUserSettings::GetAscGameUserSettings())
    {
        Settings->SetFrameRateLimit(FrameLimitOptions[Index]);
        Settings->ApplyNonResolutionSettings();
    }
}

void UAscSettingsVideoPageWidget::HandleBrightnessChanged(float Value)
{
    if (UAscGameUserSettings* Settings = UAscGameUserSettings::GetAscGameUserSettings())
    {
        Settings->SetBrightnessGamma(Value);
        Settings->ApplyNonResolutionSettings();
    }
}

#undef LOCTEXT_NAMESPACE
