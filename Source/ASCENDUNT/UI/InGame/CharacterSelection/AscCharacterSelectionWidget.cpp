#include "AscCharacterSelectionWidget.h"

#include "ASCENDUNT/GameModes/PVP/MatchGameState.h"
#include "ASCENDUNT/HeroSelection/AscHeroRosterData.h"
#include "ASCENDUNT/Player/AscenduntPlayerController.h"
#include "ASCENDUNT/Player/AscenduntPlayerState.h"
#include "ASCENDUNT/UI/InGame/MapIntro/AscMapIntroCatalogData.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"

void UAscCharacterSelectionWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Btn_Hero0)
    {
        Btn_Hero0->OnClicked.RemoveDynamic(this, &ThisClass::HandleHero0Clicked);
        Btn_Hero0->OnClicked.AddDynamic(this, &ThisClass::HandleHero0Clicked);
    }

    if (Btn_Hero1)
    {
        Btn_Hero1->OnClicked.RemoveDynamic(this, &ThisClass::HandleHero1Clicked);
        Btn_Hero1->OnClicked.AddDynamic(this, &ThisClass::HandleHero1Clicked);
    }

    if (Btn_Hero2)
    {
        Btn_Hero2->OnClicked.RemoveDynamic(this, &ThisClass::HandleHero2Clicked);
        Btn_Hero2->OnClicked.AddDynamic(this, &ThisClass::HandleHero2Clicked);
    }

    if (Btn_Hero3)
    {
        Btn_Hero3->OnClicked.RemoveDynamic(this, &ThisClass::HandleHero3Clicked);
        Btn_Hero3->OnClicked.AddDynamic(this, &ThisClass::HandleHero3Clicked);
    }

    if (Btn_Confirm)
    {
        Btn_Confirm->OnClicked.RemoveDynamic(this, &ThisClass::HandleConfirmClicked);
        Btn_Confirm->OnClicked.AddDynamic(this, &ThisClass::HandleConfirmClicked);
    }

    if (!OwningAscPC)
    {
        OwningAscPC = Cast<AAscenduntPlayerController>(GetOwningPlayer());
    }

    BindMatchGameState();
    BindOwningPlayerState();

    RefreshSelectionText();
    RefreshRemainingTimeText();
    RefreshCharacterSelectPresentation();
}

FReply UAscCharacterSelectionWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    if (Key == EKeys::Escape || Key == EKeys::H)
    {
        if (AAscenduntPlayerController* AscPC = OwningAscPC ? OwningAscPC.Get() : Cast<AAscenduntPlayerController>(GetOwningPlayer()))
        {
            AscPC->CloseHeroSelection();
            return FReply::Handled();
        }
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UAscCharacterSelectionWidget::NativeDestruct()
{
    if (AMatchGameState* MatchGS = CachedMatchGameState.Get())
    {
        MatchGS->OnMatchGameStateChanged.RemoveAll(this);
    }

    CachedMatchGameState.Reset();
    UnbindOwningPlayerState();

    Super::NativeDestruct();
}

void UAscCharacterSelectionWidget::BindMatchGameState()
{
    AMatchGameState* MatchGS = GetWorld() ? GetWorld()->GetGameState<AMatchGameState>() : nullptr;
    if (CachedMatchGameState.Get() == MatchGS)
    {
        return;
    }

    if (AMatchGameState* PreviousMatchGS = CachedMatchGameState.Get())
    {
        PreviousMatchGS->OnMatchGameStateChanged.RemoveAll(this);
    }

    CachedMatchGameState = MatchGS;

    if (MatchGS)
    {
        MatchGS->OnMatchGameStateChanged.RemoveAll(this);
        MatchGS->OnMatchGameStateChanged.AddUObject(this, &ThisClass::HandleMatchGameStateChanged);
    }
}

void UAscCharacterSelectionWidget::BindOwningPlayerState()
{
    AAscenduntPlayerState* AscPS = OwningAscPC
        ? OwningAscPC->GetAscenduntPS()
        : (GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<AAscenduntPlayerState>() : nullptr);

    if (CachedOwningPlayerState.Get() == AscPS)
    {
        return;
    }

    UnbindOwningPlayerState();
    CachedOwningPlayerState = AscPS;

    if (AscPS)
    {
        AscPS->OnPlayerStateChanged.RemoveAll(this);
        AscPS->OnPlayerStateChanged.AddUObject(this, &ThisClass::HandleOwningPlayerStateChanged);
    }
}

void UAscCharacterSelectionWidget::UnbindOwningPlayerState()
{
    if (AAscenduntPlayerState* AscPS = CachedOwningPlayerState.Get())
    {
        AscPS->OnPlayerStateChanged.RemoveAll(this);
    }

    CachedOwningPlayerState.Reset();
}

void UAscCharacterSelectionWidget::HandleMatchGameStateChanged()
{
    RefreshRemainingTimeText();
    RefreshCharacterSelectPresentation();
}

void UAscCharacterSelectionWidget::HandleOwningPlayerStateChanged()
{
    RefreshCharacterSelectPresentation();
}

void UAscCharacterSelectionWidget::RefreshRemainingTimeText()
{
    if (!Txt_RemainingTime)
    {
        return;
    }

    AMatchGameState* MatchGS = CachedMatchGameState.Get();
    if (!MatchGS)
    {
        BindMatchGameState();
        MatchGS = CachedMatchGameState.Get();
    }

    if (!MatchGS)
    {
        Txt_RemainingTime->SetText(FText::GetEmpty());
        return;
    }

    const EAscMatchPhase Phase = MatchGS->GetMatchPhase();
    const FAscMatchClockState& ClockState = MatchGS->GetClockState();

    if (Phase == EAscMatchPhase::CharacterSelect)
    {
        Txt_RemainingTime->SetText(FText::Format(
            FText::FromString(TEXT("남은 시간: {0}")),
            FText::AsNumber(ClockState.PhaseRemainingSeconds)));
    }
    else
    {
        Txt_RemainingTime->SetText(FText::GetEmpty());
    }
}

void UAscCharacterSelectionWidget::RefreshCharacterSelectPresentation()
{
    if (!Img_CharacterSelectBackground)
    {
        return;
    }

    // Use the same map/team background for both initial selection and in-game hero changes.
    if (!MapIntroCatalog)
    {
        Img_CharacterSelectBackground->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    if (!CachedMatchGameState.IsValid())
    {
        BindMatchGameState();
    }

    if (!CachedOwningPlayerState.IsValid())
    {
        BindOwningPlayerState();
    }

    const AMatchGameState* MatchGS = CachedMatchGameState.Get();
    const AAscenduntPlayerState* AscPS = CachedOwningPlayerState.Get();
    if (!MatchGS || !AscPS)
    {
        Img_CharacterSelectBackground->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    UTexture2D* BackgroundImage = MapIntroCatalog->GetCharacterSelectImageForTeam(
        MatchGS->GetMatchSettings().SelectedMapPath,
        AscPS->GetTeamId());

    if (!BackgroundImage)
    {
        Img_CharacterSelectBackground->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    Img_CharacterSelectBackground->SetBrushFromTexture(BackgroundImage, true);
    Img_CharacterSelectBackground->SetVisibility(ESlateVisibility::HitTestInvisible);
}

FText UAscCharacterSelectionWidget::GetHeroNameById(int32 HeroId) const
{
    if (!HeroRosterData)
    {
        return FText::Format(
            FText::FromString(TEXT("영웅 {0}")),
            FText::AsNumber(HeroId));
    }

    if (const FAscHeroDefinition* Definition = HeroRosterData->FindHeroDefinition(HeroId))
    {
        if (!Definition->HeroName.IsEmpty())
        {
            return Definition->HeroName;
        }
    }

    return FText::Format(
        FText::FromString(TEXT("영웅 {0}")),
        FText::AsNumber(HeroId));
}

void UAscCharacterSelectionWidget::InitializeCharacterSelection(AAscenduntPlayerController* InOwningAscPC, EAscHeroSelectionContext InContext)
{
    OwningAscPC = InOwningAscPC ? InOwningAscPC : Cast<AAscenduntPlayerController>(GetOwningPlayer());
    SelectionContext = InContext;
    SelectedHeroId = INDEX_NONE;

    BindMatchGameState();
    BindOwningPlayerState();

    RefreshSelectionText();
    RefreshCharacterSelectPresentation();

    UE_LOG(LogTemp, Warning,
        TEXT("[CharacterSelectionUI] Initialize PC=%s Context=%d"),
        *GetNameSafe(OwningAscPC),
        static_cast<int32>(SelectionContext));
}

void UAscCharacterSelectionWidget::SelectHero(int32 HeroId)
{
    SelectedHeroId = HeroId;
    RefreshSelectionText();
}

void UAscCharacterSelectionWidget::ConfirmSelectedHero()
{
    if (SelectedHeroId == INDEX_NONE)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CharacterSelectionUI] Confirm ignored: no hero selected."));
        return;
    }

    AAscenduntPlayerController* AscPC = OwningAscPC ? OwningAscPC.Get() : Cast<AAscenduntPlayerController>(GetOwningPlayer());
    if (!AscPC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CharacterSelectionUI] Confirm failed: owning player controller is not AAscenduntPlayerController."));
        return;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[CharacterSelectionUI] Confirm HeroId=%d Context=%d"),
        SelectedHeroId,
        static_cast<int32>(SelectionContext));

    AscPC->Server_RequestConfirmHero(SelectedHeroId, SelectionContext);
}

void UAscCharacterSelectionWidget::HandleHero0Clicked()
{
    SelectHero(0);
}

void UAscCharacterSelectionWidget::HandleHero1Clicked()
{
    SelectHero(1);
}

void UAscCharacterSelectionWidget::HandleHero2Clicked()
{
    SelectHero(2);
}

void UAscCharacterSelectionWidget::HandleHero3Clicked()
{
    SelectHero(3);
}

void UAscCharacterSelectionWidget::HandleConfirmClicked()
{
    ConfirmSelectedHero();
}

void UAscCharacterSelectionWidget::RefreshSelectionText()
{
    if (!Txt_SelectedHero)
    {
        return;
    }

    if (SelectedHeroId == INDEX_NONE)
    {
        Txt_SelectedHero->SetText(FText::FromString(TEXT("영웅을 선택해주세요")));
    }
    else
    {
        Txt_SelectedHero->SetText(FText::Format(
            FText::FromString(TEXT("선택한 영웅 : {0}")),
            GetHeroNameById(SelectedHeroId)));
    }
}
