#include "AscAbilitySlotWidget.h"

#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/CanvasPanel.h"
#include "GameFramework/Pawn.h"

void UAscAbilitySlotWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Progress_CooldownOverlay)
    {
        Progress_CooldownOverlay->SetFillColorAndOpacity(CooldownOverlayColor);
    }

    if (Txt_Key)
    {
        Txt_Key->SetText(KeyText);
    }

    RefreshAbilityUI();
}

void UAscAbilitySlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshAbilityUI();
}

void UAscAbilitySlotWidget::SetAbilityInputTag(FGameplayTag InInputTag)
{
    AbilityInputTag = InInputTag;
    RefreshAbilityUI();
}

UAscenduntAbilitySystemComponent* UAscAbilitySlotWidget::ResolveASC() const
{
    APawn* OwningPawn = GetOwningPlayerPawn();
    if (!OwningPawn)
    {
        return nullptr;
    }

    return Cast<UAscenduntAbilitySystemComponent>(
        UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwningPawn));
}

void UAscAbilitySlotWidget::RefreshAbilityUI()
{
    FAscAbilityUIState State;

    UAscenduntAbilitySystemComponent* ASC = ResolveASC();

    const bool bHasState =
        ASC &&
        AbilityInputTag.IsValid() &&
        ASC->GetAbilityUIStateByInputTag(AbilityInputTag, State);

    if (!bHasState)
    {
        if (SlotRoot)
        {
            SlotRoot->SetVisibility(ESlateVisibility::Collapsed);
        }

        return;
    }

    if (SlotRoot)
    {
        SlotRoot->SetVisibility(ESlateVisibility::HitTestInvisible);
    }

    // 아이콘
    if (Image_AbilityIcon)
    {
        Image_AbilityIcon->SetBrushFromTexture(State.AbilityIcon, false);
    }

    // 스톡
    if (Txt_Stock)
    {
        Txt_Stock->SetVisibility(
            State.bUsesStock
            ? ESlateVisibility::HitTestInvisible
            : ESlateVisibility::Collapsed);

        if (State.bUsesStock)
        {
            Txt_Stock->SetText(FText::AsNumber(State.CurrentStock));
        }
    }

    // 쿨타임 오버레이
    if (CooldownOverlay)
    {
        CooldownOverlay->SetVisibility(
            State.bShowCooldownOverlay
            ? ESlateVisibility::HitTestInvisible
            : ESlateVisibility::Collapsed);
    }

    if (Progress_CooldownOverlay)
    {
        Progress_CooldownOverlay->SetPercent(State.OverlayPercent);
    }

    // 남은 쿨타임
    if (Txt_CooldownRemaining)
    {
        Txt_CooldownRemaining->SetVisibility(
            State.bShowCooldownOverlay
            ? ESlateVisibility::HitTestInvisible
            : ESlateVisibility::Collapsed);

        if (State.bShowCooldownOverlay)
        {
            Txt_CooldownRemaining->SetText(
                FText::AsNumber(
                    FMath::Max(1, FMath::CeilToInt(State.TimeRemaining))
                )
            );
        }
    }

    BP_OnAbilityUIStateChanged(
        State.bUsesStock,
        State.CurrentStock,
        State.MaxStock,
        State.bShowCooldownOverlay,
        State.TimeRemaining,
        State.Duration,
        State.OverlayPercent);
}
