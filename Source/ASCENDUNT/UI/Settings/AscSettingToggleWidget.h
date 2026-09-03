#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AscSettingToggleWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAscSettingToggleChanged, bool, bEnabled);

/** Reusable ON/OFF row. The visual may be fully restyled in WBP. */
UCLASS()
class ASCENDUNT_API UAscSettingToggleWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Settings|Row")
    void ConfigureToggle(const FText& InName, const FText& InDescription, bool bInEnabled);

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Settings|Row")
    void SetEnabledValue(bool bInEnabled, bool bBroadcast = false);

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Settings|Row")
    bool GetEnabledValue() const { return bEnabled; }

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Settings|Row")
    FAscSettingToggleChanged OnToggleChanged;

protected:
    virtual void NativeConstruct() override;

    UFUNCTION()
    void HandleToggleClicked();

    void RefreshText();

protected:
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_Name;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_Description;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_Value;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_Toggle;

private:
    bool bEnabled = false;
};
