#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "AscGameUserSettings.generated.h"

UENUM(BlueprintType)
enum class EAscAntiAliasingMethod : uint8
{
    FXAA UMETA(DisplayName = "FXAA"),
    TAA  UMETA(DisplayName = "TAA"),
    TSR  UMETA(DisplayName = "TSR")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAscGameUserSettingsApplied);

/**
 * ASCENDUNT-specific local user settings.
 *
 * Engine-owned display/scalability settings (resolution, fullscreen mode,
 * VSync, frame-rate limit, quality groups, etc.) continue to use the
 * UGameUserSettings API. This class only adds game-specific values that the
 * engine base class does not own.
 */
UCLASS(Config = GameUserSettings)
class ASCENDUNT_API UAscGameUserSettings : public UGameUserSettings
{
    GENERATED_BODY()

public:
    UAscGameUserSettings(const FObjectInitializer& ObjectInitializer);

    /** Returns the global ASCENDUNT user-settings instance. */
    UFUNCTION(BlueprintPure, Category = "Ascendunt|Settings", meta = (DisplayName = "Get ASCENDUNT Game User Settings"))
    static UAscGameUserSettings* GetAscGameUserSettings();

    virtual void SetToDefaults() override;
    virtual void ValidateSettings() override;
    virtual void ApplyNonResolutionSettings() override;

    /** Applies all pending engine + ASCENDUNT settings and persists them. */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Settings")
    void ApplyAndSaveSettings(bool bCheckForCommandLineOverrides = false);

    // ---------------------------------------------------------------------
    // Graphics
    // ---------------------------------------------------------------------

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Settings|Graphics")
    EAscAntiAliasingMethod GetAntiAliasingMethod() const { return AntiAliasingMethod; }

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Settings|Graphics")
    void SetAntiAliasingMethod(EAscAntiAliasingMethod NewMethod);

    /** Display gamma used for the user-facing brightness slider. 2.2 is neutral. */
    UFUNCTION(BlueprintPure, Category = "Ascendunt|Settings|Graphics")
    float GetBrightnessGamma() const { return BrightnessGamma; }

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Settings|Graphics")
    void SetBrightnessGamma(float NewGamma);

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Settings|Graphics")
    bool IsMotionBlurEnabled() const { return bMotionBlurEnabled; }

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Settings|Graphics")
    void SetMotionBlurEnabled(bool bEnabled);

    // ---------------------------------------------------------------------
    // Controls
    // ---------------------------------------------------------------------

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Settings|Controls")
    float GetMouseSensitivity() const { return MouseSensitivity; }

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Settings|Controls")
    void SetMouseSensitivity(float NewSensitivity);

    /** User sensitivity multiplier applied while ADS. This is the only ADS-specific sensitivity scale. */
    UFUNCTION(BlueprintPure, Category = "Ascendunt|Settings|Controls")
    float GetADSSensitivityMultiplier() const { return ADSSensitivityMultiplier; }

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Settings|Controls")
    void SetADSSensitivityMultiplier(float NewMultiplier);

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Settings|Controls")
    bool IsInvertYAxisEnabled() const { return bInvertYAxis; }

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Settings|Controls")
    void SetInvertYAxisEnabled(bool bEnabled);

    // ---------------------------------------------------------------------
    // Audio
    // ---------------------------------------------------------------------

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Settings|Audio")
    float GetMasterVolume() const { return MasterVolume; }

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Settings|Audio")
    void SetMasterVolume(float NewVolume);

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Settings|Audio")
    float GetMusicVolume() const { return MusicVolume; }

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Settings|Audio")
    void SetMusicVolume(float NewVolume);

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Settings|Audio")
    float GetSFXVolume() const { return SFXVolume; }

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Settings|Audio")
    void SetSFXVolume(float NewVolume);

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Settings|Audio")
    float GetVoiceVolume() const { return VoiceVolume; }

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Settings|Audio")
    void SetVoiceVolume(float NewVolume);

    // ---------------------------------------------------------------------
    // First-run setup
    // ---------------------------------------------------------------------

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Settings|Setup")
    bool IsInitialSetupCompleted() const { return bInitialSetupCompleted; }

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Settings|Setup")
    void SetInitialSetupCompleted(bool bCompleted) { bInitialSetupCompleted = bCompleted; }

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Settings")
    FAscGameUserSettingsApplied OnSettingsApplied;

private:
    void ApplyAntiAliasingMethod() const;
    void ApplyBrightnessGamma() const;
    void ApplyMotionBlurSetting() const;

private:
    UPROPERTY(Config)
    EAscAntiAliasingMethod AntiAliasingMethod = EAscAntiAliasingMethod::TSR;

    UPROPERTY(Config)
    float BrightnessGamma = 2.2f;

    UPROPERTY(Config)
    bool bMotionBlurEnabled = false;

    UPROPERTY(Config)
    float MouseSensitivity = 1.0f;

    UPROPERTY(Config)
    float ADSSensitivityMultiplier = 1.0f;

    UPROPERTY(Config)
    bool bInvertYAxis = false;

    UPROPERTY(Config)
    float MasterVolume = 1.0f;

    UPROPERTY(Config)
    float MusicVolume = 1.0f;

    UPROPERTY(Config)
    float SFXVolume = 1.0f;

    UPROPERTY(Config)
    float VoiceVolume = 1.0f;

    UPROPERTY(Config)
    bool bInitialSetupCompleted = false;
};
