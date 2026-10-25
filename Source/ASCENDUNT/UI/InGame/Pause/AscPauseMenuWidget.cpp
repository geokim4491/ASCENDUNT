#include "AscPauseMenuWidget.h"

#include "ASCENDUNT/Player/AscenduntPlayerController.h"
#include "Components/Button.h"
#include "InputCoreTypes.h"

void UAscPauseMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    SetIsFocusable(true);

    if (Btn_Settings)
    {
        Btn_Settings->OnClicked.RemoveDynamic(this, &ThisClass::HandleSettingsClicked);
        Btn_Settings->OnClicked.AddDynamic(this, &ThisClass::HandleSettingsClicked);
    }
    if (Btn_ExitGame)
    {
        Btn_ExitGame->OnClicked.RemoveDynamic(this, &ThisClass::HandleExitGameClicked);
        Btn_ExitGame->OnClicked.AddDynamic(this, &ThisClass::HandleExitGameClicked);
    }

    if (!OwningAscPC)
    {
        OwningAscPC = Cast<AAscenduntPlayerController>(GetOwningPlayer());
    }

    SetKeyboardFocus();
}

void UAscPauseMenuWidget::InitializePauseMenu(AAscenduntPlayerController* InOwningPC)
{
    OwningAscPC = InOwningPC;
}

FReply UAscPauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Escape)
    {
        if (AAscenduntPlayerController* AscPC = OwningAscPC ? OwningAscPC.Get() : Cast<AAscenduntPlayerController>(GetOwningPlayer()))
        {
            AscPC->ClosePauseMenu();
            return FReply::Handled();
        }
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UAscPauseMenuWidget::HandleSettingsClicked()
{
    if (AAscenduntPlayerController* AscPC = OwningAscPC ? OwningAscPC.Get() : Cast<AAscenduntPlayerController>(GetOwningPlayer()))
    {
        AscPC->OpenSettingsMenu();
    }
}

void UAscPauseMenuWidget::HandleExitGameClicked()
{
    if (AAscenduntPlayerController* AscPC = OwningAscPC ? OwningAscPC.Get() : Cast<AAscenduntPlayerController>(GetOwningPlayer()))
    {
        AscPC->LeaveCurrentMatchToMainMenu();
    }
}
