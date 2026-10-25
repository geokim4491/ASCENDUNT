// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AscMainMenuTypes.h"
#include "ASCENDUNT/Match/AscMatchTypes.h"
#include "AscMainMenuWidget.generated.h"

class UWidgetSwitcher;
class UTextBlock;
class UAscMainMenuHomePageWidget;
class UAscQuickMatchSubsystem;

/**
 * Main menu shell after removing the old room/session UI.
 *
 * This class intentionally keeps only the root/home-page behavior. Quick Match
 * pages should be added later as new widgets instead of reusing the removed
 * CreateRoom / SessionBrowser / Lobby UI classes.
 */
UCLASS()
class ASCENDUNT_API UAscMainMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|MainMenu")
    void ShowPage(EAscMainMenuPage InPage);

    UFUNCTION(BlueprintPure, Category = "Ascendunt|MainMenu")
    EAscMainMenuPage GetCurrentPage() const { return CurrentPage; }

protected:
    /**
     * One-shot event fired after returning from a successful PvE mission.
     * Implement in the main-menu WBP to cover the menu with a victory video/sequence.
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Ascendunt|MainMenu|PvE")
    void BP_OnPvEMissionSuccessReturn();

    UFUNCTION()
    void HandleHomeRequestOpenPlay();

    UFUNCTION()
    void HandleHomeRequestQuickMatchTest();

    UFUNCTION()
    void HandleQuickMatchQueueStateChanged(const FAscClientQueueState& InQueueState);

    UFUNCTION()
    void HandleQuickMatchError(const FString& ErrorMessage);

    UFUNCTION()
    void HandleMatchServerAssigned(const FAscMatchServerConnectionInfo& ConnectionInfo);

protected:
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidgetSwitcher> MenuSwitcher;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UAscMainMenuHomePageWidget> Page_Main;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Txt_StatusMessage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|MainMenu")
    bool bDisableUnimplementedButtons = true;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Ascendunt|MainMenu")
    EAscMainMenuPage CurrentPage = EAscMainMenuPage::Main;

private:
    FTimerHandle StatusMessageClearTimerHandle;

private:
    void InitializePages();
    void BindPageDelegates();
    void ApplyInitialState();
    void UpdatePageActivationState(EAscMainMenuPage OldPage, EAscMainMenuPage NewPage);
    void SetStatusMessage(const FText& InText, float ShowTime = 3.f);
    bool IsPageIndexValid(EAscMainMenuPage InPage) const;
    UAscQuickMatchSubsystem* GetQuickMatchSubsystem() const;
    void BindQuickMatchSubsystemDelegates();
    void UnbindQuickMatchSubsystemDelegates();
    FText BuildQueueStatusText(const FAscClientQueueState& InQueueState) const;
};
