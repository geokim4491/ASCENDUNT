#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AscSettingsWidget.generated.h"

class AAscenduntPlayerController;
class UButton;
class UWidgetSwitcher;
class UAscSettingsPageWidgetBase;

UENUM(BlueprintType)
enum class EAscSettingsPage : uint8
{
    Video = 0,
    Graphics,
    Controls,
    Audio
};

/** Root settings screen opened from the in-match ESC menu. */
UCLASS()
class ASCENDUNT_API UAscSettingsWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Settings")
    void InitializeSettings(AAscenduntPlayerController* InOwningPC);

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Settings")
    void ShowPage(EAscSettingsPage Page);

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Settings")
    void RefreshAllPages();

protected:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    UFUNCTION() void HandleVideoClicked();
    UFUNCTION() void HandleGraphicsClicked();
    UFUNCTION() void HandleControlsClicked();
    UFUNCTION() void HandleAudioClicked();
    UFUNCTION() void HandleDefaultClicked();
    UFUNCTION() void HandleApplyClicked();
    UFUNCTION() void HandleBackClicked();

protected:
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Video;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Graphics;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Controls;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Audio;

    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidgetSwitcher> SettingsSwitcher;

    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UAscSettingsPageWidgetBase> Page_Video;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UAscSettingsPageWidgetBase> Page_Graphics;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UAscSettingsPageWidgetBase> Page_Controls;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UAscSettingsPageWidgetBase> Page_Audio;

    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Default;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Apply;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Back;

private:
    UPROPERTY(Transient)
    TObjectPtr<AAscenduntPlayerController> OwningAscPC;

    EAscSettingsPage CurrentPage = EAscSettingsPage::Video;
};
