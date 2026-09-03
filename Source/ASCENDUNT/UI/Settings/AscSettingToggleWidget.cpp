#include "AscSettingToggleWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void UAscSettingToggleWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Btn_Toggle)
    {
        Btn_Toggle->OnClicked.RemoveDynamic(this, &ThisClass::HandleToggleClicked);
        Btn_Toggle->OnClicked.AddDynamic(this, &ThisClass::HandleToggleClicked);
    }

    RefreshText();
}

void UAscSettingToggleWidget::ConfigureToggle(const FText& InName, const FText& InDescription, bool bInEnabled)
{
    if (Txt_Name)
    {
        Txt_Name->SetText(InName);
    }
    if (Txt_Description)
    {
        Txt_Description->SetText(InDescription);
    }

    SetEnabledValue(bInEnabled, false);
}

void UAscSettingToggleWidget::SetEnabledValue(bool bInEnabled, bool bBroadcast)
{
    bEnabled = bInEnabled;
    RefreshText();

    if (bBroadcast)
    {
        OnToggleChanged.Broadcast(bEnabled);
    }
}

void UAscSettingToggleWidget::HandleToggleClicked()
{
    SetEnabledValue(!bEnabled, true);
}

void UAscSettingToggleWidget::RefreshText()
{
    if (Txt_Value)
    {
        Txt_Value->SetText(bEnabled ? FText::FromString(TEXT("ON")) : FText::FromString(TEXT("OFF")));
    }
}
