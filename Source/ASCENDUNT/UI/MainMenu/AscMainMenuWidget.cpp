#include "AscMainMenuWidget.h"

#include "AscMainMenuHomePageWidget.h"
#include "ASCENDUNT/GameModes/TestAndSettings/AscGameInstance.h"
#include "ASCENDUNT/System/AscQuickMatchSubsystem.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "AscMainMenuWidget"

void UAscMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    SetIsFocusable(true);

    InitializePages();
    BindPageDelegates();
    BindQuickMatchSubsystemDelegates();
    ApplyInitialState();

    SetKeyboardFocus();

    if (UAscGameInstance* AscGameInstance = Cast<UAscGameInstance>(GetGameInstance()))
    {
        if (AscGameInstance->ConsumePendingMainMenuPresentation()
            == EAscPendingMainMenuPresentation::PvEMissionSuccess)
        {
            BP_OnPvEMissionSuccessReturn();
        }
    }
}

void UAscMainMenuWidget::NativeDestruct()
{
    UnbindQuickMatchSubsystemDelegates();

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(StatusMessageClearTimerHandle);
    }

    Super::NativeDestruct();
}

FReply UAscMainMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Escape && CurrentPage != EAscMainMenuPage::Main)
    {
        ShowPage(EAscMainMenuPage::Main);
        return FReply::Handled();
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UAscMainMenuWidget::ShowPage(EAscMainMenuPage InPage)
{
    if (!MenuSwitcher)
    {
        return;
    }

    if (!IsPageIndexValid(InPage))
    {
        // The old Play/CreateRoom/SessionBrowser pages were removed. Keep the
        // menu on Main until the new Quick Match page is implemented.
        const bool bRequestedMain = (InPage == EAscMainMenuPage::Main);
        InPage = EAscMainMenuPage::Main;

        if (!bRequestedMain)
        {
            SetStatusMessage(LOCTEXT("QuickMatchNotImplemented", "빠른 시작 UI는 새 구조로 다시 구현 예정입니다."));
        }
    }

    const EAscMainMenuPage OldPage = CurrentPage;
    CurrentPage = InPage;

    MenuSwitcher->SetActiveWidgetIndex(static_cast<int32>(InPage));
    UpdatePageActivationState(OldPage, InPage);

    if (InPage == EAscMainMenuPage::Main && OldPage != EAscMainMenuPage::Main)
    {
        SetStatusMessage(FText::GetEmpty());
    }
}

void UAscMainMenuWidget::HandleHomeRequestOpenPlay()
{
    // The previous Play page depended on the removed room/session UI. For now,
    // keep the action contained and let the future Quick Match widget replace it.
    ShowPage(EAscMainMenuPage::Play);
}

void UAscMainMenuWidget::HandleHomeRequestQuickMatchTest()
{
    UAscQuickMatchSubsystem* QuickMatchSubsystem = GetQuickMatchSubsystem();
    if (!QuickMatchSubsystem)
    {
        SetStatusMessage(LOCTEXT("QuickMatchSubsystemMissing", "빠른 시작 Subsystem을 찾을 수 없습니다."));
        return;
    }

    const FAscClientQueueState& QueueState = QuickMatchSubsystem->GetQueueState();
    if (QueueState.IsWaiting())
    {
        QuickMatchSubsystem->CancelQueue();
        SetStatusMessage(LOCTEXT("QuickMatchCancelRequested", "빠른 시작 대기 취소 요청 중..."));
        return;
    }

    if (QueueState.CanTravelToMatch())
    {
        QuickMatchSubsystem->TravelToMatchServer();
        return;
    }

    // 테스트 단계에서는 Due1v1만 고정 호출한다. 이후 QuickMatchPage에서 모드 선택 UI를 붙이면 됨.
    QuickMatchSubsystem->JoinQueue(EAscMatchMode::Duel1v1, TEXT("TestPlayer"));
    SetStatusMessage(LOCTEXT("QuickMatchJoinRequested", "1v1 빠른 시작 요청 중..."));
}

void UAscMainMenuWidget::HandleQuickMatchQueueStateChanged(const FAscClientQueueState& InQueueState)
{
    SetStatusMessage(BuildQueueStatusText(InQueueState), 9999.f);
}

void UAscMainMenuWidget::HandleQuickMatchError(const FString& ErrorMessage)
{
    SetStatusMessage(FText::FromString(FString::Printf(TEXT("빠른 시작 오류: %s"), *ErrorMessage)), 5.f);
}

void UAscMainMenuWidget::HandleMatchServerAssigned(const FAscMatchServerConnectionInfo& ConnectionInfo)
{
    const FString MapName = ConnectionInfo.SelectedMapDisplayName.IsEmpty()
        ? ConnectionInfo.SelectedMapPath
        : ConnectionInfo.SelectedMapDisplayName;

    SetStatusMessage(FText::FromString(FString::Printf(
        TEXT("매치 서버 배정됨: %s (%s)"),
        *ConnectionInfo.MatchServerTravelAddress,
        *MapName)),
        5.f);
}

void UAscMainMenuWidget::InitializePages()
{
    if (Page_Main)
    {
        Page_Main->InitializePage(this);
    }
}

void UAscMainMenuWidget::BindPageDelegates()
{
    if (Page_Main)
    {
        Page_Main->OnRequestOpenPlay.RemoveDynamic(this, &ThisClass::HandleHomeRequestOpenPlay);
        Page_Main->OnRequestOpenPlay.AddDynamic(this, &ThisClass::HandleHomeRequestOpenPlay);

        Page_Main->OnRequestQuickMatchTest.RemoveDynamic(this, &ThisClass::HandleHomeRequestQuickMatchTest);
        Page_Main->OnRequestQuickMatchTest.AddDynamic(this, &ThisClass::HandleHomeRequestQuickMatchTest);
    }
}

void UAscMainMenuWidget::ApplyInitialState()
{
    if (Page_Main)
    {
        Page_Main->SetUnimplementedButtonsEnabled(!bDisableUnimplementedButtons);
    }

    CurrentPage = EAscMainMenuPage::Main;
    ShowPage(EAscMainMenuPage::Main);
    SetStatusMessage(FText::GetEmpty());
}

void UAscMainMenuWidget::UpdatePageActivationState(EAscMainMenuPage OldPage, EAscMainMenuPage NewPage)
{
    if (OldPage == NewPage)
    {
        return;
    }

    if (OldPage == EAscMainMenuPage::Main && Page_Main)
    {
        Page_Main->OnPageDeactivated();
    }

    if (NewPage == EAscMainMenuPage::Main && Page_Main)
    {
        Page_Main->OnPageActivated();
    }
}

void UAscMainMenuWidget::SetStatusMessage(const FText& InText, float ShowTime)
{
    if (!Txt_StatusMessage)
    {
        return;
    }

    Txt_StatusMessage->SetText(InText);

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(StatusMessageClearTimerHandle);

        if (!InText.IsEmpty())
        {
            GetWorld()->GetTimerManager().SetTimer(
                StatusMessageClearTimerHandle,
                [this]()
                {
                    if (Txt_StatusMessage)
                    {
                        Txt_StatusMessage->SetText(FText::GetEmpty());
                    }
                },
                ShowTime,
                false);
        }
    }
}

bool UAscMainMenuWidget::IsPageIndexValid(EAscMainMenuPage InPage) const
{
    if (!MenuSwitcher)
    {
        return false;
    }

    const int32 PageIndex = static_cast<int32>(InPage);
    return PageIndex >= 0 && PageIndex < MenuSwitcher->GetChildrenCount();
}

UAscQuickMatchSubsystem* UAscMainMenuWidget::GetQuickMatchSubsystem() const
{
    UGameInstance* GameInstance = GetGameInstance();
    return GameInstance ? GameInstance->GetSubsystem<UAscQuickMatchSubsystem>() : nullptr;
}

void UAscMainMenuWidget::BindQuickMatchSubsystemDelegates()
{
    UAscQuickMatchSubsystem* QuickMatchSubsystem = GetQuickMatchSubsystem();
    if (!QuickMatchSubsystem)
    {
        return;
    }

    QuickMatchSubsystem->OnQueueStateChanged.RemoveDynamic(this, &ThisClass::HandleQuickMatchQueueStateChanged);
    QuickMatchSubsystem->OnQueueStateChanged.AddDynamic(this, &ThisClass::HandleQuickMatchQueueStateChanged);

    QuickMatchSubsystem->OnQueueError.RemoveDynamic(this, &ThisClass::HandleQuickMatchError);
    QuickMatchSubsystem->OnQueueError.AddDynamic(this, &ThisClass::HandleQuickMatchError);

    QuickMatchSubsystem->OnMatchServerAssigned.RemoveDynamic(this, &ThisClass::HandleMatchServerAssigned);
    QuickMatchSubsystem->OnMatchServerAssigned.AddDynamic(this, &ThisClass::HandleMatchServerAssigned);
}

void UAscMainMenuWidget::UnbindQuickMatchSubsystemDelegates()
{
    UAscQuickMatchSubsystem* QuickMatchSubsystem = GetQuickMatchSubsystem();
    if (!QuickMatchSubsystem)
    {
        return;
    }

    QuickMatchSubsystem->OnQueueStateChanged.RemoveDynamic(this, &ThisClass::HandleQuickMatchQueueStateChanged);
    QuickMatchSubsystem->OnQueueError.RemoveDynamic(this, &ThisClass::HandleQuickMatchError);
    QuickMatchSubsystem->OnMatchServerAssigned.RemoveDynamic(this, &ThisClass::HandleMatchServerAssigned);
}

FText UAscMainMenuWidget::BuildQueueStatusText(const FAscClientQueueState& InQueueState) const
{
    switch (InQueueState.Status)
    {
    case EAscQueueTicketStatus::None:
        return FText::GetEmpty();

    case EAscQueueTicketStatus::Queued:
        return FText::FromString(FString::Printf(
            TEXT("빠른 시작 대기 중: %d/%d"),
            InQueueState.CurrentPlayers,
            InQueueState.RequiredPlayers));

    case EAscQueueTicketStatus::Launching:
        return FText::FromString(FString::Printf(
            TEXT("매치 서버 준비 중: %d/%d"),
            InQueueState.CurrentPlayers,
            InQueueState.RequiredPlayers));

    case EAscQueueTicketStatus::Ready:
        return FText::FromString(FString::Printf(
            TEXT("매치 서버 준비 완료: %s"),
            *InQueueState.Connection.MatchServerTravelAddress));

    case EAscQueueTicketStatus::Cancelled:
        return LOCTEXT("QuickMatchCancelled", "빠른 시작 대기가 취소되었습니다.");

    case EAscQueueTicketStatus::Failed:
        return FText::FromString(FString::Printf(
            TEXT("빠른 시작 실패: %s"),
            *InQueueState.ErrorMessage));

    case EAscQueueTicketStatus::Expired:
        return LOCTEXT("QuickMatchExpired", "빠른 시작 대기 시간이 만료되었습니다.");

    default:
        return FText::GetEmpty();
    }
}

#undef LOCTEXT_NAMESPACE
