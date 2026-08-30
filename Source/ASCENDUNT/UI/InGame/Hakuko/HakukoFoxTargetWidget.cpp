#include "HakukoFoxTargetWidget.h"
#include "Components/TextBlock.h"

void UHakukoFoxTargetWidget::SetFoxTargetInfo(EHakukoFoxType InFoxType)
{
	SetStimTargetInfo(FText::GetEmpty());
	K2_OnFoxTargetInfoChanged(InFoxType);
}

void UHakukoFoxTargetWidget::SetStimTargetInfo(const FText& TargetName)
{
	if (Txt_TargetName) Txt_TargetName->SetText(TargetName);
	else if (Txt_FoxType) Txt_FoxType->SetText(TargetName);
	if (Txt_Action) Txt_Action->SetText(FText::FromString(TEXT("[Shift] 전투 자극제")));
	K2_OnStimTargetInfoChanged(TargetName);
}
