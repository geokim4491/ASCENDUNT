#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AscSettingSelectorWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAscSettingSelectionChanged, int32, SelectedIndex);

/** Reusable left/right discrete option row used by the ASCENDUNT settings menu. */
UCLASS()
class ASCENDUNT_API UAscSettingSelectorWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Settings|Row")
    void ConfigureSelector(const FText& InName, const FText& InDescription, const TArray<FText>& InOptions, int32 InSelectedIndex);

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Settings|Row")
    void SetOptions(const TArray<FText>& InOptions, int32 InSelectedIndex);

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Settings|Row")
    void SetSelectedIndex(int32 InSelectedIndex, bool bBroadcast = false);

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Settings|Row")
    int32 GetSelectedIndex() const { return SelectedIndex; }

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Settings|Row")
    FAscSettingSelectionChanged OnSelectionChanged;

protected:
    virtual void NativeConstruct() override;

    UFUNCTION()
    void HandleLeftClicked();

    UFUNCTION()
    void HandleRightClicked();

    void RefreshText();

protected:
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_Name;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_Description;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_Value;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_Left;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_Right;

private:
    UPROPERTY(Transient)
    TArray<FText> Options;

    int32 SelectedIndex = INDEX_NONE;
};
