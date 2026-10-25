#include "AscSettingsWidget.h"

#include "AscSettingsPageWidgetBase.h"
#include "ASCENDUNT/Player/AscenduntPlayerController.h"
#include "ASCENDUNT/System/AscGameUserSettings.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "InputCoreTypes.h"

void UAscSettingsWidget::NativeConstruct()
{
    Super::NativeConstruct();

    SetIsFocusable(true);

#define BIND_BUTTON(Button, Handler) \
    if (Button) { Button->OnClicked.RemoveDynamic(this, &ThisClass::Handler); Button->OnClicked.AddDynamic(this, &ThisClass::Handler); }

    BIND_BUTTON(Btn_Video, HandleVideoClicked);
    BIND_BUTTON(Btn_Graphics, HandleGraphicsClicked);
    BIND_BUTTON(Btn_Controls, HandleControlsClicked);
    BIND_BUTTON(Btn_Audio, HandleAudioClicked);
    BIND_BUTTON(Btn_Default, HandleDefaultClicked);
    BIND_BUTTON(Btn_Apply, HandleApplyClicked);
    BIND_BUTTON(Btn_Back, HandleBackClicked);
#undef BIND_BUTTON

    if (!OwningAscPC)
    {
        OwningAscPC = Cast<AAscenduntPlayerController>(GetOwningPlayer());
    }

    RefreshAllPages();
    ShowPage(CurrentPage);
    SetKeyboardFocus();
}

void UAscSettingsWidget::InitializeSettings(AAscenduntPlayerController* InOwningPC)
{
    OwningAscPC = InOwningPC;
    RefreshAllPages();
    ShowPage(CurrentPage);
}

FReply UAscSettingsWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Escape)
    {
        HandleBackClicked();
        return FReply::Handled();
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UAscSettingsWidget::ShowPage(EAscSettingsPage Page)
{
    CurrentPage = Page;
    if (SettingsSwitcher)
    {
        const int32 PageIndex = static_cast<int32>(Page);
        if (PageIndex >= 0 && PageIndex < SettingsSwitcher->GetChildrenCount())
        {
            SettingsSwitcher->SetActiveWidgetIndex(PageIndex);
        }
    }
}

void UAscSettingsWidget::RefreshAllPages()
{
    if (Page_Video) Page_Video->RefreshFromSettings();
    if (Page_Graphics) Page_Graphics->RefreshFromSettings();
    if (Page_Controls) Page_Controls->RefreshFromSettings();
    if (Page_Audio) Page_Audio->RefreshFromSettings();
}

void UAscSettingsWidget::HandleVideoClicked() { ShowPage(EAscSettingsPage::Video); }
void UAscSettingsWidget::HandleGraphicsClicked() { ShowPage(EAscSettingsPage::Graphics); }
void UAscSettingsWidget::HandleControlsClicked() { ShowPage(EAscSettingsPage::Controls); }
void UAscSettingsWidget::HandleAudioClicked() { ShowPage(EAscSettingsPage::Audio); }

void UAscSettingsWidget::HandleDefaultClicked()
{
    if (UAscGameUserSettings* Settings = UAscGameUserSettings::GetAscGameUserSettings())
    {
        Settings->SetToDefaults();
        Settings->ApplyNonResolutionSettings();
        RefreshAllPages();
    }
}

void UAscSettingsWidget::HandleApplyClicked()
{
    if (UAscGameUserSettings* Settings = UAscGameUserSettings::GetAscGameUserSettings())
    {
        Settings->ApplyAndSaveSettings(false);
        RefreshAllPages();
    }
}

void UAscSettingsWidget::HandleBackClicked()
{
    if (AAscenduntPlayerController* AscPC = OwningAscPC ? OwningAscPC.Get() : Cast<AAscenduntPlayerController>(GetOwningPlayer()))
    {
        AscPC->CloseSettingsMenu();
    }
}
