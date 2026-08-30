// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AscMainMenuPageWidgetBase.h"
#include "AscMainMenuHomePageWidget.generated.h"

class UButton;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAscOnHomePageRequestOpenPlay);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAscOnHomePageRequestQuickMatchTest);

/**
 * 
 */
UCLASS()
class ASCENDUNT_API UAscMainMenuHomePageWidget : public UAscMainMenuPageWidgetBase
{
	GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    void SetUnimplementedButtonsEnabled(bool bEnabled);

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|MainMenu|Event")
    FAscOnHomePageRequestOpenPlay OnRequestOpenPlay;

    /** 테스트용 빠른 시작 버튼 요청. 현재는 기존 Play 버튼도 이 이벤트를 호출함. */
    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|MainMenu|Event")
    FAscOnHomePageRequestQuickMatchTest OnRequestQuickMatchTest;

protected:
    UFUNCTION()
    void HandlePlayClicked();

    UFUNCTION()
    void HandleQuickMatchTestClicked();

protected:
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Btn_Play;

    /** BP에 이 이름의 버튼을 추가하면 별도 테스트 버튼으로 동작함. 없으면 Btn_Play를 테스트 버튼처럼 사용함. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_QuickMatchTest;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_Hero;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_Settings;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_SocialProfile;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_QuitGame;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Btn_Profile;
};
