#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AscPauseMenuWidget.generated.h"

class AAscenduntPlayerController;
class UButton;

/** Local-only in-match ESC menu. Dedicated server gameplay is never paused. */
UCLASS()
class ASCENDUNT_API UAscPauseMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Pause Menu")
    void InitializePauseMenu(AAscenduntPlayerController* InOwningPC);

protected:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    UFUNCTION() void HandleSettingsClicked();
    UFUNCTION() void HandleExitGameClicked();

protected:
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Settings;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_ExitGame;

private:
    UPROPERTY(Transient)
    TObjectPtr<AAscenduntPlayerController> OwningAscPC;
};
