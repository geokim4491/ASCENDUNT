#include "AscSettingsAudioPageWidget.h"

#include "AscSettingSliderWidget.h"
#include "ASCENDUNT/System/AscGameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

#define LOCTEXT_NAMESPACE "AscSettingsAudioPage"

void UAscSettingsAudioPageWidget::NativeConstruct()
{
    Super::NativeConstruct();

#define BIND_SLIDER(Widget, Handler) \
    if (Widget) { Widget->OnValueChanged.RemoveDynamic(this, &ThisClass::Handler); Widget->OnValueChanged.AddDynamic(this, &ThisClass::Handler); }

    BIND_SLIDER(Setting_MasterVolume, HandleMasterChanged);
    BIND_SLIDER(Setting_MusicVolume, HandleMusicChanged);
    BIND_SLIDER(Setting_SFXVolume, HandleSFXChanged);
    BIND_SLIDER(Setting_VoiceVolume, HandleVoiceChanged);
#undef BIND_SLIDER

    if (SettingsSoundMix)
    {
        UGameplayStatics::PushSoundMixModifier(this, SettingsSoundMix);
    }

    RefreshFromSettings();
}

void UAscSettingsAudioPageWidget::RefreshFromSettings()
{
    const UAscGameUserSettings* Settings = UAscGameUserSettings::GetAscGameUserSettings();
    if (!Settings)
    {
        return;
    }

    auto ConfigureVolume = [](UAscSettingSliderWidget* Widget, const FText& Name, const FText& Description, float Value)
    {
        if (Widget)
        {
            Widget->ConfigureSlider(Name, Description, 0.0f, 1.0f, 0.01f, 100.0f, 0, FText::FromString(TEXT("%")), Value);
        }
    };

    ConfigureVolume(Setting_MasterVolume, LOCTEXT("MasterVolume", "MASTER VOLUME"), LOCTEXT("MasterVolumeDesc", "Overall game volume."), Settings->GetMasterVolume());
    ConfigureVolume(Setting_MusicVolume, LOCTEXT("MusicVolume", "MUSIC VOLUME"), LOCTEXT("MusicVolumeDesc", "Music volume."), Settings->GetMusicVolume());
    ConfigureVolume(Setting_SFXVolume, LOCTEXT("SFXVolume", "SFX VOLUME"), LOCTEXT("SFXVolumeDesc", "Weapons, abilities, and world effects volume."), Settings->GetSFXVolume());
    ConfigureVolume(Setting_VoiceVolume, LOCTEXT("VoiceVolume", "VOICE VOLUME"), LOCTEXT("VoiceVolumeDesc", "Character and announcer voice volume."), Settings->GetVoiceVolume());

    ApplySoundClassVolume(MasterSoundClass, Settings->GetMasterVolume());
    ApplySoundClassVolume(MusicSoundClass, Settings->GetMusicVolume());
    ApplySoundClassVolume(SFXSoundClass, Settings->GetSFXVolume());
    ApplySoundClassVolume(VoiceSoundClass, Settings->GetVoiceVolume());
}

void UAscSettingsAudioPageWidget::ApplySoundClassVolume(USoundClass* SoundClass, float Volume)
{
    if (SettingsSoundMix && SoundClass)
    {
        UGameplayStatics::SetSoundMixClassOverride(this, SettingsSoundMix, SoundClass, Volume, 1.0f, 0.0f, true);
    }
}

void UAscSettingsAudioPageWidget::HandleMasterChanged(float Value)
{
    if (UAscGameUserSettings* Settings = UAscGameUserSettings::GetAscGameUserSettings()) { Settings->SetMasterVolume(Value); ApplySoundClassVolume(MasterSoundClass, Value); }
}
void UAscSettingsAudioPageWidget::HandleMusicChanged(float Value)
{
    if (UAscGameUserSettings* Settings = UAscGameUserSettings::GetAscGameUserSettings()) { Settings->SetMusicVolume(Value); ApplySoundClassVolume(MusicSoundClass, Value); }
}
void UAscSettingsAudioPageWidget::HandleSFXChanged(float Value)
{
    if (UAscGameUserSettings* Settings = UAscGameUserSettings::GetAscGameUserSettings()) { Settings->SetSFXVolume(Value); ApplySoundClassVolume(SFXSoundClass, Value); }
}
void UAscSettingsAudioPageWidget::HandleVoiceChanged(float Value)
{
    if (UAscGameUserSettings* Settings = UAscGameUserSettings::GetAscGameUserSettings()) { Settings->SetVoiceVolume(Value); ApplySoundClassVolume(VoiceSoundClass, Value); }
}

#undef LOCTEXT_NAMESPACE
