#include "AscGameUserSettings.h"

#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

namespace AscGameUserSettings
{
    constexpr float MinBrightnessGamma = 1.6f;
    constexpr float MaxBrightnessGamma = 3.0f;

    constexpr float MinMouseSensitivity = 0.05f;
    constexpr float MaxMouseSensitivity = 5.0f;

    constexpr float MinADSSensitivityMultiplier = 0.05f;
    constexpr float MaxADSSensitivityMultiplier = 2.0f;

    constexpr float MinVolume = 0.0f;
    constexpr float MaxVolume = 1.0f;
}

UAscGameUserSettings::UAscGameUserSettings(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

UAscGameUserSettings* UAscGameUserSettings::GetAscGameUserSettings()
{
    if (!GEngine)
    {
        return nullptr;
    }

    UGameUserSettings* BaseSettings = GEngine->GetGameUserSettings();
    UAscGameUserSettings* AscSettings = Cast<UAscGameUserSettings>(BaseSettings);

    if (!AscSettings && BaseSettings)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[AscGameUserSettings] Active GameUserSettings class is %s, not UAscGameUserSettings. "
                 "Set Project Settings > Engine > General Settings > Game User Settings Class to AscGameUserSettings."),
            *GetNameSafe(BaseSettings->GetClass()));
    }

    return AscSettings;
}

void UAscGameUserSettings::SetToDefaults()
{
    Super::SetToDefaults();

    AntiAliasingMethod = EAscAntiAliasingMethod::TSR;
    BrightnessGamma = 2.2f;
    bMotionBlurEnabled = false;

    MouseSensitivity = 1.0f;
    ADSSensitivityMultiplier = 1.0f;
    bInvertYAxis = false;

    MasterVolume = 1.0f;
    MusicVolume = 1.0f;
    SFXVolume = 1.0f;
    VoiceVolume = 1.0f;

    // Do not reset bInitialSetupCompleted here. Resetting ordinary options should
    // not make the first-run setup appear again on the next launch.
}

void UAscGameUserSettings::ValidateSettings()
{
    Super::ValidateSettings();

    switch (AntiAliasingMethod)
    {
    case EAscAntiAliasingMethod::FXAA:
    case EAscAntiAliasingMethod::TAA:
    case EAscAntiAliasingMethod::TSR:
        break;

    default:
        AntiAliasingMethod = EAscAntiAliasingMethod::TSR;
        break;
    }

    BrightnessGamma = FMath::Clamp(
        BrightnessGamma,
        AscGameUserSettings::MinBrightnessGamma,
        AscGameUserSettings::MaxBrightnessGamma);

    MouseSensitivity = FMath::Clamp(
        MouseSensitivity,
        AscGameUserSettings::MinMouseSensitivity,
        AscGameUserSettings::MaxMouseSensitivity);

    ADSSensitivityMultiplier = FMath::Clamp(
        ADSSensitivityMultiplier,
        AscGameUserSettings::MinADSSensitivityMultiplier,
        AscGameUserSettings::MaxADSSensitivityMultiplier);

    MasterVolume = FMath::Clamp(MasterVolume, AscGameUserSettings::MinVolume, AscGameUserSettings::MaxVolume);
    MusicVolume = FMath::Clamp(MusicVolume, AscGameUserSettings::MinVolume, AscGameUserSettings::MaxVolume);
    SFXVolume = FMath::Clamp(SFXVolume, AscGameUserSettings::MinVolume, AscGameUserSettings::MaxVolume);
    VoiceVolume = FMath::Clamp(VoiceVolume, AscGameUserSettings::MinVolume, AscGameUserSettings::MaxVolume);
}

void UAscGameUserSettings::ApplyNonResolutionSettings()
{
    Super::ApplyNonResolutionSettings();

    ValidateSettings();
    ApplyAntiAliasingMethod();
    ApplyBrightnessGamma();
    ApplyMotionBlurSetting();
}

void UAscGameUserSettings::ApplyAndSaveSettings(bool bCheckForCommandLineOverrides)
{
    // UGameUserSettings::ApplySettings applies resolution + non-resolution
    // settings and also persists GameUserSettings.ini.
    ApplySettings(bCheckForCommandLineOverrides);
    OnSettingsApplied.Broadcast();
}

void UAscGameUserSettings::SetAntiAliasingMethod(EAscAntiAliasingMethod NewMethod)
{
    AntiAliasingMethod = NewMethod;
}

void UAscGameUserSettings::SetBrightnessGamma(float NewGamma)
{
    BrightnessGamma = FMath::Clamp(
        NewGamma,
        AscGameUserSettings::MinBrightnessGamma,
        AscGameUserSettings::MaxBrightnessGamma);
}

void UAscGameUserSettings::SetMotionBlurEnabled(bool bEnabled)
{
    bMotionBlurEnabled = bEnabled;
}

void UAscGameUserSettings::SetMouseSensitivity(float NewSensitivity)
{
    MouseSensitivity = FMath::Clamp(
        NewSensitivity,
        AscGameUserSettings::MinMouseSensitivity,
        AscGameUserSettings::MaxMouseSensitivity);
}

void UAscGameUserSettings::SetADSSensitivityMultiplier(float NewMultiplier)
{
    ADSSensitivityMultiplier = FMath::Clamp(
        NewMultiplier,
        AscGameUserSettings::MinADSSensitivityMultiplier,
        AscGameUserSettings::MaxADSSensitivityMultiplier);
}

void UAscGameUserSettings::SetInvertYAxisEnabled(bool bEnabled)
{
    bInvertYAxis = bEnabled;
}

void UAscGameUserSettings::SetMasterVolume(float NewVolume)
{
    MasterVolume = FMath::Clamp(NewVolume, AscGameUserSettings::MinVolume, AscGameUserSettings::MaxVolume);
}

void UAscGameUserSettings::SetMusicVolume(float NewVolume)
{
    MusicVolume = FMath::Clamp(NewVolume, AscGameUserSettings::MinVolume, AscGameUserSettings::MaxVolume);
}

void UAscGameUserSettings::SetSFXVolume(float NewVolume)
{
    SFXVolume = FMath::Clamp(NewVolume, AscGameUserSettings::MinVolume, AscGameUserSettings::MaxVolume);
}

void UAscGameUserSettings::SetVoiceVolume(float NewVolume)
{
    VoiceVolume = FMath::Clamp(NewVolume, AscGameUserSettings::MinVolume, AscGameUserSettings::MaxVolume);
}

void UAscGameUserSettings::ApplyAntiAliasingMethod() const
{
    IConsoleVariable* AntiAliasingMethodCVar =
        IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod"));

    if (!AntiAliasingMethodCVar)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AscGameUserSettings] r.AntiAliasingMethod was not found."));
        return;
    }

    int32 MethodValue = 4; // TSR
    switch (AntiAliasingMethod)
    {
    case EAscAntiAliasingMethod::FXAA:
        MethodValue = 1;
        break;

    case EAscAntiAliasingMethod::TAA:
        MethodValue = 2;
        break;

    case EAscAntiAliasingMethod::TSR:
    default:
        MethodValue = 4;
        break;
    }

    AntiAliasingMethodCVar->Set(MethodValue, ECVF_SetByGameSetting);
}

void UAscGameUserSettings::ApplyBrightnessGamma() const
{
    if (GEngine)
    {
        GEngine->DisplayGamma = BrightnessGamma;
    }
}

void UAscGameUserSettings::ApplyMotionBlurSetting() const
{
    // This controls the engine default. A camera or post-process volume that
    // explicitly overrides MotionBlurAmount can still take precedence.
    IConsoleVariable* MotionBlurCVar =
        IConsoleManager::Get().FindConsoleVariable(TEXT("r.DefaultFeature.MotionBlur"));

    if (!MotionBlurCVar)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AscGameUserSettings] r.DefaultFeature.MotionBlur was not found."));
        return;
    }

    MotionBlurCVar->Set(bMotionBlurEnabled ? 1 : 0, ECVF_SetByGameSetting);
}
