#include "AscMapIntroPlayerEntryWidget.h"

#include "Components/TextBlock.h"

void UAscMapIntroPlayerEntryWidget::SetPlayerName(const FString& InPlayerName)
{
    if (Txt_PlayerName)
    {
        Txt_PlayerName->SetText(FText::FromString(InPlayerName));
    }
}
