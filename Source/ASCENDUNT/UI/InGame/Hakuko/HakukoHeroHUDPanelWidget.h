#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/UI/InGame/Hero/AscHeroHUDPanelWidget.h"
#include "HakukoHeroHUDPanelWidget.generated.h"

class UHakukoSkillComponent;
class UHakukoFoxTargetWidget;
class UImage;
class UTextBlock;
class UProgressBar;

UCLASS()
class ASCENDUNT_API UHakukoHeroHUDPanelWidget : public UAscHeroHUDPanelWidget
{
	GENERATED_BODY()
public:
	UHakukoHeroHUDPanelWidget(const FObjectInitializer& ObjectInitializer);
	virtual void NotifyOwningPawnChanged(APawn* NewPawn) override;
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION() void HandleStimTargetChanged(AActor* TargetActor);
	UFUNCTION() void HandleHealGaugeChanged(float NewGauge);
	void BindSkillComponent(APawn* NewPawn);
	void UnbindSkillComponent();
	void RefreshUI();
	FText ResolveTargetDisplayText(AActor* TargetActor) const;

	// Legacy bindings remain optional so the existing WBP can be migrated without breaking load.
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="Ascendunt|Hero HUD|Hakuko") TObjectPtr<UTextBlock> Txt_SelectedFox;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="Ascendunt|Hero HUD|Hakuko") TObjectPtr<UTextBlock> Txt_AttackFoxTarget;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="Ascendunt|Hero HUD|Hakuko") TObjectPtr<UTextBlock> Txt_SupportFoxTarget;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="Ascendunt|Hero HUD|Hakuko") TObjectPtr<UImage> Img_AttackFoxSelected;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="Ascendunt|Hero HUD|Hakuko") TObjectPtr<UImage> Img_SupportFoxSelected;

	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="Ascendunt|Hero HUD|Hakuko") TObjectPtr<UHakukoFoxTargetWidget> FoxTargetPrompt;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="Ascendunt|Hero HUD|Hakuko") TObjectPtr<UProgressBar> Progress_HealGrenadeGauge;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="Ascendunt|Hero HUD|Hakuko") TObjectPtr<UTextBlock> Txt_HealGrenadeGauge;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ascendunt|Hero HUD|Hakuko") FText NoTargetLabel;
private:
	TWeakObjectPtr<UHakukoSkillComponent> BoundSkillComponent;
};
