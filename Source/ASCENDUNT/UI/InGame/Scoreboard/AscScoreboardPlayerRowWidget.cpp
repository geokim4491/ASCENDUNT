#include "AscScoreboardPlayerRowWidget.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

namespace AscScoreboardRowInternal
{
    static FText RoundedNumber(float Value)
    {
        return FText::AsNumber(FMath::RoundToInt(Value));
    }
}

void UAscScoreboardPlayerRowWidget::SetEntry(const FAscScoreboardPlayerEntry& InEntry)
{
    Entry = InEntry;

    if (Img_Portrait)
    {
        if (Entry.Portrait)
        {
            Img_Portrait->SetBrushFromTexture(Entry.Portrait, true);
            Img_Portrait->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else
        {
            Img_Portrait->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    if (Txt_PlayerName)       Txt_PlayerName->SetText(Entry.PlayerName);
    if (Txt_MonsterKills)     Txt_MonsterKills->SetText(FText::AsNumber(Entry.MonsterKills));
    if (Txt_DownedCount)      Txt_DownedCount->SetText(FText::AsNumber(Entry.DownedCount));
    if (Txt_ReviveCount)      Txt_ReviveCount->SetText(FText::AsNumber(Entry.ReviveCount));
    if (Txt_DamageDealt)      Txt_DamageDealt->SetText(AscScoreboardRowInternal::RoundedNumber(Entry.DamageDealt));
    if (Txt_HealingDone)      Txt_HealingDone->SetText(AscScoreboardRowInternal::RoundedNumber(Entry.HealingDone));
    if (Txt_HealingReceived)  Txt_HealingReceived->SetText(AscScoreboardRowInternal::RoundedNumber(Entry.HealingReceived));

    if (Progress_Ultimate)
    {
        Progress_Ultimate->SetPercent(FMath::Clamp(Entry.UltimateChargePercent, 0.0f, 1.0f));
    }

    if (Txt_UltimatePercent)
    {
        Txt_UltimatePercent->SetText(FText::Format(
            FText::FromString(TEXT("{0}%")),
            FText::AsNumber(FMath::RoundToInt(FMath::Clamp(Entry.UltimateChargePercent, 0.0f, 1.0f) * 100.0f))));
    }

    if (LocalPlayerHighlight)
    {
        LocalPlayerHighlight->SetVisibility(
            Entry.bIsLocalPlayer ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
}
