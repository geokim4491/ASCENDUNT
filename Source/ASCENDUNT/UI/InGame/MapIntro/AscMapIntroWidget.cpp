#include "AscMapIntroWidget.h"

#include "ASCENDUNT/GameModes/PVP/MatchGameState.h"
#include "ASCENDUNT/Player/AscenduntPlayerState.h"
#include "AscMapIntroCatalogData.h"
#include "AscMapIntroPlayerEntryWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"

namespace AscMapIntroWidgetPrivate
{
    struct FPlayerNameEntry
    {
        int32 PlayerId = INDEX_NONE;
        FString Name;
    };

    void SortByPlayerId(TArray<FPlayerNameEntry>& Entries)
    {
        Entries.Sort(
            [](const FPlayerNameEntry& A, const FPlayerNameEntry& B)
            {
                return A.PlayerId < B.PlayerId;
            });
    }
}

void UAscMapIntroWidget::InitializeMapIntro(AMatchGameState* InMatchGameState)
{
    BindMatchGameState(InMatchGameState);
    RefreshMapPresentation();
    RebuildRoster();
}

void UAscMapIntroWidget::NativeConstruct()
{
    Super::NativeConstruct();

    RefreshMapPresentation();
    RebuildRoster();
}

void UAscMapIntroWidget::NativeDestruct()
{
    UnbindMatchGameState();
    Super::NativeDestruct();
}

void UAscMapIntroWidget::BindMatchGameState(AMatchGameState* InMatchGameState)
{
    if (CachedMatchGameState.Get() == InMatchGameState)
    {
        return;
    }

    UnbindMatchGameState();
    CachedMatchGameState = InMatchGameState;
    CachedPresentationMapPath.Reset();

    if (InMatchGameState)
    {
        InMatchGameState->OnPlayerRosterChanged.RemoveAll(this);
        InMatchGameState->OnPlayerRosterChanged.AddUObject(
            this,
            &ThisClass::HandlePlayerRosterChanged);

        InMatchGameState->OnMatchGameStateChanged.RemoveAll(this);
        InMatchGameState->OnMatchGameStateChanged.AddUObject(
            this,
            &ThisClass::HandleMatchGameStateChanged);
    }
}

void UAscMapIntroWidget::UnbindMatchGameState()
{
    if (AMatchGameState* MatchGS = CachedMatchGameState.Get())
    {
        MatchGS->OnPlayerRosterChanged.RemoveAll(this);
        MatchGS->OnMatchGameStateChanged.RemoveAll(this);
    }

    CachedMatchGameState.Reset();
    CachedPresentationMapPath.Reset();
}

void UAscMapIntroWidget::HandlePlayerRosterChanged()
{
    RebuildRoster();
}

void UAscMapIntroWidget::HandleMatchGameStateChanged()
{
    RefreshMapPresentation();
}

void UAscMapIntroWidget::RefreshMapPresentation()
{
    const AMatchGameState* MatchGS = CachedMatchGameState.Get();
    if (!MatchGS || !MapIntroCatalog)
    {
        return;
    }

    const FAscMatchRuntimeSettings& MatchSettings = MatchGS->GetMatchSettings();
    if (CachedPresentationMapPath == MatchSettings.SelectedMapPath)
    {
        return;
    }

    CachedPresentationMapPath = MatchSettings.SelectedMapPath;

    const FAscMapIntroPresentation* Presentation =
        MapIntroCatalog->FindPresentationNative(MatchSettings.SelectedMapPath);

    if (!Presentation)
    {
        if (Img_MapBackground)
        {
            Img_MapBackground->SetVisibility(ESlateVisibility::Collapsed);
        }

        if (Txt_MapName)
        {
            Txt_MapName->SetText(FText::FromString(MatchSettings.SelectedMapDisplayName));
        }
        return;
    }

    if (Img_MapBackground)
    {
        Img_MapBackground->SetBrushFromTexture(Presentation->BackgroundImage, true);
        Img_MapBackground->SetVisibility(
            Presentation->BackgroundImage
                ? ESlateVisibility::HitTestInvisible
                : ESlateVisibility::Collapsed);
    }

    if (Txt_MapName)
    {
        const FText DisplayName = Presentation->MapDisplayName.IsEmpty()
            ? FText::FromString(MatchSettings.SelectedMapDisplayName)
            : Presentation->MapDisplayName;

        Txt_MapName->SetText(DisplayName);
    }
}

void UAscMapIntroWidget::RebuildRoster()
{
    const AMatchGameState* MatchGS = CachedMatchGameState.Get();
    if (!MatchGS)
    {
        if (VB_TeamOnePlayers)
        {
            VB_TeamOnePlayers->ClearChildren();
        }
        if (VB_TeamTwoPlayers)
        {
            VB_TeamTwoPlayers->ClearChildren();
        }
        return;
    }

    TArray<AscMapIntroWidgetPrivate::FPlayerNameEntry> TeamOneEntries;
    TArray<AscMapIntroWidgetPrivate::FPlayerNameEntry> TeamTwoEntries;

    for (APlayerState* BasePlayerState : MatchGS->PlayerArray)
    {
        const AAscenduntPlayerState* AscPlayerState =
            Cast<AAscenduntPlayerState>(BasePlayerState);
        if (!AscPlayerState)
        {
            continue;
        }

        FString DisplayName = AscPlayerState->GetDisplayName();
        if (DisplayName.IsEmpty())
        {
            DisplayName = AscPlayerState->GetPlayerName();
        }

        AscMapIntroWidgetPrivate::FPlayerNameEntry Entry;
        Entry.PlayerId = AscPlayerState->GetPlayerId();
        Entry.Name = MoveTemp(DisplayName);

        if (AscPlayerState->GetTeamId() == 1)
        {
            TeamOneEntries.Add(MoveTemp(Entry));
        }
        else if (AscPlayerState->GetTeamId() == 2)
        {
            TeamTwoEntries.Add(MoveTemp(Entry));
        }
    }

    AscMapIntroWidgetPrivate::SortByPlayerId(TeamOneEntries);
    AscMapIntroWidgetPrivate::SortByPlayerId(TeamTwoEntries);

    TArray<FString> TeamOneNames;
    TArray<FString> TeamTwoNames;
    TeamOneNames.Reserve(TeamOneEntries.Num());
    TeamTwoNames.Reserve(TeamTwoEntries.Num());

    for (const AscMapIntroWidgetPrivate::FPlayerNameEntry& Entry : TeamOneEntries)
    {
        TeamOneNames.Add(Entry.Name);
    }

    for (const AscMapIntroWidgetPrivate::FPlayerNameEntry& Entry : TeamTwoEntries)
    {
        TeamTwoNames.Add(Entry.Name);
    }

    RebuildTeamBox(VB_TeamOnePlayers, TeamOneNames);
    RebuildTeamBox(VB_TeamTwoPlayers, TeamTwoNames);
}

void UAscMapIntroWidget::RebuildTeamBox(
    UVerticalBox* TargetBox,
    const TArray<FString>& PlayerNames)
{
    if (!TargetBox)
    {
        return;
    }

    TargetBox->ClearChildren();

    if (!PlayerEntryWidgetClass)
    {
        return;
    }

    for (const FString& PlayerName : PlayerNames)
    {
        UAscMapIntroPlayerEntryWidget* EntryWidget =
            CreateWidget<UAscMapIntroPlayerEntryWidget>(
                GetOwningPlayer(),
                PlayerEntryWidgetClass);
        if (!EntryWidget)
        {
            continue;
        }

        EntryWidget->SetPlayerName(PlayerName);
        TargetBox->AddChildToVerticalBox(EntryWidget);
    }
}
