// Fill out your copyright notice in the Description page of Project Settings.


#include "AscMainMenuHomePageWidget.h"

#include "Components/Button.h"

void UAscMainMenuHomePageWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Btn_Play)
    {
        Btn_Play->OnClicked.RemoveDynamic(this, &ThisClass::HandlePlayClicked);
        Btn_Play->OnClicked.AddDynamic(this, &ThisClass::HandlePlayClicked);
    }

    if (Btn_QuickMatchTest)
    {
        Btn_QuickMatchTest->OnClicked.RemoveDynamic(this, &ThisClass::HandleQuickMatchTestClicked);
        Btn_QuickMatchTest->OnClicked.AddDynamic(this, &ThisClass::HandleQuickMatchTestClicked);
    }
}

void UAscMainMenuHomePageWidget::SetUnimplementedButtonsEnabled(bool bEnabled)
{
    if (Btn_Hero)
    {
        Btn_Hero->SetIsEnabled(bEnabled);
    }

    if (Btn_Settings)
    {
        Btn_Settings->SetIsEnabled(bEnabled);
    }

    if (Btn_SocialProfile)
    {
        Btn_SocialProfile->SetIsEnabled(bEnabled);
    }

    if (Btn_QuitGame)
    {
        Btn_QuitGame->SetIsEnabled(bEnabled);
    }

    if (Btn_Profile)
    {
        Btn_Profile->SetIsEnabled(bEnabled);
    }
}

void UAscMainMenuHomePageWidget::HandlePlayClicked()
{
    // 기존 Play 페이지는 삭제되었으므로, 테스트 단계에서는 Play 버튼을 빠른 시작 테스트 버튼으로 사용한다.
    HandleQuickMatchTestClicked();
}

void UAscMainMenuHomePageWidget::HandleQuickMatchTestClicked()
{
    OnRequestQuickMatchTest.Broadcast();
}
