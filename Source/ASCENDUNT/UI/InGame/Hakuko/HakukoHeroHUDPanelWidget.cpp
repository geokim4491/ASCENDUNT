#include "HakukoHeroHUDPanelWidget.h"

#include "ASCENDUNT/Character/Hero/Hakuko/HakukoSkillComponent.h"
#include "ASCENDUNT/UI/InGame/Hakuko/HakukoFoxTargetWidget.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

UHakukoHeroHUDPanelWidget::UHakukoHeroHUDPanelWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NoTargetLabel = FText::FromString(TEXT("없음"));
}

void UHakukoHeroHUDPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindSkillComponent(GetObservedPawn() ? GetObservedPawn() : GetOwningPlayerPawn());
}
void UHakukoHeroHUDPanelWidget::NativeDestruct()
{
	UnbindSkillComponent();
	Super::NativeDestruct();
}
void UHakukoHeroHUDPanelWidget::NotifyOwningPawnChanged(APawn* NewPawn)
{
	Super::NotifyOwningPawnChanged(NewPawn);
	BindSkillComponent(NewPawn);
}

void UHakukoHeroHUDPanelWidget::BindSkillComponent(APawn* NewPawn)
{
	UHakukoSkillComponent* NewComp = NewPawn ? NewPawn->FindComponentByClass<UHakukoSkillComponent>() : nullptr;
	if (BoundSkillComponent.Get() == NewComp) { RefreshUI(); return; }
	UnbindSkillComponent();
	BoundSkillComponent = NewComp;
	if (NewComp)
	{
		NewComp->OnStimTargetChanged.AddUniqueDynamic(this, &ThisClass::HandleStimTargetChanged);
		NewComp->OnHealGrenadeGaugeChanged.AddUniqueDynamic(this, &ThisClass::HandleHealGaugeChanged);
	}
	RefreshUI();
}

void UHakukoHeroHUDPanelWidget::UnbindSkillComponent()
{
	if (UHakukoSkillComponent* Comp = BoundSkillComponent.Get())
	{
		Comp->OnStimTargetChanged.RemoveDynamic(this, &ThisClass::HandleStimTargetChanged);
		Comp->OnHealGrenadeGaugeChanged.RemoveDynamic(this, &ThisClass::HandleHealGaugeChanged);
	}
	BoundSkillComponent.Reset();
}

void UHakukoHeroHUDPanelWidget::HandleStimTargetChanged(AActor* TargetActor)
{
	if (!FoxTargetPrompt) return;
	if (!IsValid(TargetActor)) { FoxTargetPrompt->SetVisibility(ESlateVisibility::Collapsed); return; }
	FoxTargetPrompt->SetStimTargetInfo(ResolveTargetDisplayText(TargetActor));
	FoxTargetPrompt->SetVisibility(ESlateVisibility::HitTestInvisible);
}
void UHakukoHeroHUDPanelWidget::HandleHealGaugeChanged(float NewGauge)
{
	const float ClampedGauge = FMath::Clamp(NewGauge, 0.f, 100.f);
	if (Progress_HealGrenadeGauge)
	{
		Progress_HealGrenadeGauge->SetPercent(ClampedGauge / 100.f);
	}
	if (Txt_HealGrenadeGauge)
	{
		Txt_HealGrenadeGauge->SetText(FText::AsNumber(FMath::RoundToInt(ClampedGauge)));
	}
}

void UHakukoHeroHUDPanelWidget::RefreshUI()
{
	UHakukoSkillComponent* Comp = BoundSkillComponent.Get();
	SetVisibility(Comp ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (Txt_SelectedFox) Txt_SelectedFox->SetVisibility(ESlateVisibility::Collapsed);
	if (Txt_AttackFoxTarget) Txt_AttackFoxTarget->SetVisibility(ESlateVisibility::Collapsed);
	if (Txt_SupportFoxTarget) Txt_SupportFoxTarget->SetVisibility(ESlateVisibility::Collapsed);
	if (Img_AttackFoxSelected) Img_AttackFoxSelected->SetVisibility(ESlateVisibility::Collapsed);
	if (Img_SupportFoxSelected) Img_SupportFoxSelected->SetVisibility(ESlateVisibility::Collapsed);
	if (!Comp)
	{
		if (FoxTargetPrompt) FoxTargetPrompt->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	HandleHealGaugeChanged(Comp->GetHealGrenadeGauge());
	HandleStimTargetChanged(Comp->GetLocalStimTargetActor());
}

FText UHakukoHeroHUDPanelWidget::ResolveTargetDisplayText(AActor* TargetActor) const
{
	if (!IsValid(TargetActor)) return NoTargetLabel;
	if (const APawn* Pawn = Cast<APawn>(TargetActor))
	{
		if (const APlayerState* PS = Pawn->GetPlayerState())
		{
			const FString Name = PS->GetPlayerName();
			if (!Name.IsEmpty()) return FText::FromString(Name);
		}
	}
	return FText::FromString(TargetActor->GetName());
}
