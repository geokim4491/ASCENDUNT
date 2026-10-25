#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AscSettingSliderWidget.generated.h"

class USlider;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAscSettingSliderChanged, float, Value);

/** Reusable scalar setting row with configurable range, stepping and display formatting. */
UCLASS()
class ASCENDUNT_API UAscSettingSliderWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Settings|Row")
    void ConfigureSlider(
        const FText& InName,
        const FText& InDescription,
        float InMinValue,
        float InMaxValue,
        float InStepSize,
        float InDisplayMultiplier,
        int32 InDecimalPlaces,
        const FText& InSuffix,
        float InValue);

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Settings|Row")
    void SetValue(float InValue, bool bBroadcast = false);

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Settings|Row")
    float GetValue() const { return CurrentValue; }

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Settings|Row")
    FAscSettingSliderChanged OnValueChanged;

protected:
    virtual void NativeConstruct() override;

    UFUNCTION()
    void HandleSliderValueChanged(float NewValue);

    float SnapValue(float InValue) const;
    void RefreshValueText();

protected:
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_Name;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_Description;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_Value;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USlider> Slider_Value;

private:
    float MinValue = 0.0f;
    float MaxValue = 1.0f;
    float StepSize = 0.01f;
    float DisplayMultiplier = 1.0f;
    int32 DecimalPlaces = 2;
    FText Suffix;
    float CurrentValue = 0.0f;
    bool bUpdatingSliderInternally = false;
};
