#pragma once

#include "CoreMinimal.h"
#include "AscSettingsPageWidgetBase.h"
#include "AscSettingsAudioPageWidget.generated.h"

class UAscSettingSliderWidget;
class USoundClass;
class USoundMix;

UCLASS()
class ASCENDUNT_API UAscSettingsAudioPageWidget : public UAscSettingsPageWidgetBase
{
    GENERATED_BODY()

public:
    virtual void RefreshFromSettings() override;

protected:
    virtual void NativeConstruct() override;

    UFUNCTION() void HandleMasterChanged(float Value);
    UFUNCTION() void HandleMusicChanged(float Value);
    UFUNCTION() void HandleSFXChanged(float Value);
    UFUNCTION() void HandleVoiceChanged(float Value);

    void ApplySoundClassVolume(USoundClass* SoundClass, float Volume);

protected:
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UAscSettingSliderWidget> Setting_MasterVolume;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UAscSettingSliderWidget> Setting_MusicVolume;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UAscSettingSliderWidget> Setting_SFXVolume;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UAscSettingSliderWidget> Setting_VoiceVolume;

    /** Optional. Assign these in WBP/Defaults once your audio routing assets are ready. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Settings|Audio") TObjectPtr<USoundMix> SettingsSoundMix;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Settings|Audio") TObjectPtr<USoundClass> MasterSoundClass;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Settings|Audio") TObjectPtr<USoundClass> MusicSoundClass;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Settings|Audio") TObjectPtr<USoundClass> SFXSoundClass;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Settings|Audio") TObjectPtr<USoundClass> VoiceSoundClass;
};
