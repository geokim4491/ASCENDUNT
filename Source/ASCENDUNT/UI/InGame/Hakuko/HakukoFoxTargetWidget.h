#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASCENDUNT/Character/Hero/Hakuko/HakukoTypes.h"
#include "HakukoFoxTargetWidget.generated.h"

class UTextBlock;

/** Existing widget asset is reused as Hakuko's Stim target prompt. */
UCLASS()
class ASCENDUNT_API UHakukoFoxTargetWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	/** Legacy API retained for existing BP calls. */
	UFUNCTION(BlueprintCallable, Category="Hakuko|UI")
	void SetFoxTargetInfo(EHakukoFoxType InFoxType);

	UFUNCTION(BlueprintCallable, Category="Hakuko|UI")
	void SetStimTargetInfo(const FText& TargetName);

protected:
	UFUNCTION(BlueprintImplementableEvent, Category="Hakuko|UI", DisplayName="On Fox Target Info Changed")
	void K2_OnFoxTargetInfoChanged(EHakukoFoxType InFoxType);

	UFUNCTION(BlueprintImplementableEvent, Category="Hakuko|UI", DisplayName="On Stim Target Info Changed")
	void K2_OnStimTargetInfoChanged(const FText& TargetName);

	/** Old fox-type text can be reused as the target name without changing the WBP hierarchy. */
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="Hakuko|UI")
	TObjectPtr<UTextBlock> Txt_FoxType;

	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="Hakuko|UI")
	TObjectPtr<UTextBlock> Txt_TargetName;

	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="Hakuko|UI")
	TObjectPtr<UTextBlock> Txt_Action;
};
