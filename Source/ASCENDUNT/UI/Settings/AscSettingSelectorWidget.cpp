#include "AscSettingSelectorWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void UAscSettingSelectorWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Btn_Left)
    {
        Btn_Left->OnClicked.RemoveDynamic(this, &ThisClass::HandleLeftClicked);
        Btn_Left->OnClicked.AddDynamic(this, &ThisClass::HandleLeftClicked);
    }

    if (Btn_Right)
    {
        Btn_Right->OnClicked.RemoveDynamic(this, &ThisClass::HandleRightClicked);
        Btn_Right->OnClicked.AddDynamic(this, &ThisClass::HandleRightClicked);
    }

    RefreshText();
}

void UAscSettingSelectorWidget::ConfigureSelector(
    const FText& InName,
    const FText& InDescription,
    const TArray<FText>& InOptions,
    int32 InSelectedIndex)
{
    if (Txt_Name)
    {
        Txt_Name->SetText(InName);
    }
    if (Txt_Description)
    {
        Txt_Description->SetText(InDescription);
    }

    SetOptions(InOptions, InSelectedIndex);
}

void UAscSettingSelectorWidget::SetOptions(const TArray<FText>& InOptions, int32 InSelectedIndex)
{
    Options = InOptions;
    SetSelectedIndex(InSelectedIndex, false);
}

void UAscSettingSelectorWidget::SetSelectedIndex(int32 InSelectedIndex, bool bBroadcast)
{
    if (Options.IsEmpty())
    {
        SelectedIndex = INDEX_NONE;
    }
    else
    {
        SelectedIndex = FMath::Clamp(InSelectedIndex, 0, Options.Num() - 1);
    }

    RefreshText();

    if (bBroadcast && SelectedIndex != INDEX_NONE)
    {
        OnSelectionChanged.Broadcast(SelectedIndex);
    }
}

void UAscSettingSelectorWidget::HandleLeftClicked()
{
    if (Options.IsEmpty())
    {
        return;
    }

    const int32 NewIndex = (SelectedIndex <= 0) ? Options.Num() - 1 : SelectedIndex - 1;
    SetSelectedIndex(NewIndex, true);
}

void UAscSettingSelectorWidget::HandleRightClicked()
{
    if (Options.IsEmpty())
    {
        return;
    }

    const int32 NewIndex = (SelectedIndex + 1) % Options.Num();
    SetSelectedIndex(NewIndex, true);
}

void UAscSettingSelectorWidget::RefreshText()
{
    if (Txt_Value)
    {
        Txt_Value->SetText(Options.IsValidIndex(SelectedIndex) ? Options[SelectedIndex] : FText::GetEmpty());
    }
}
