#include "AscInGameHUDWidget.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntWeaponSet.h"

#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntUltimateSet.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/Character/Hero/HeroCharacter.h"
#include "ASCENDUNT/Character/Hero/PVE/AscPvEDownedComponent.h"
#include "ASCENDUNT/Interaction/AscInteractionComponent.h"
#include "ASCENDUNT/Interaction/AscInteractable.h"
#include "ASCENDUNT/GameModes/PVP/MatchGameState.h"
#include "ASCENDUNT/HeroSelection/AscHeroRosterData.h"
#include "ASCENDUNT/Player/AscenduntPlayerController.h"
#include "ASCENDUNT/Player/AscenduntPlayerState.h"
#include "ASCENDUNT/UI/InGame/MapIntro/AscMapIntroWidget.h"
#include "ASCENDUNT/UI/InGame/Mission/AscMissionObjectiveWidget.h"
#include "ASCENDUNT/UI/InGame/HealthBar/AscPlayerHealthPanelWidget.h"
#include "ASCENDUNT/UI/InGame/Hero/AscHeroHUDPanelWidget.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Weapon/WeaponEquipmentComponent.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"

UAscInGameHUDWidget::UAscInGameHUDWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    InteractionTapFormat = FText::FromString(TEXT("[V] {0}"));
    InteractionHoldFormat = FText::FromString(TEXT("[V] 길게 눌러 {0}"));
    PreRoundCountdownFormat = FText::FromString(TEXT("게임 시작까지 {0}"));
    PlayingStartMessage = FText::FromString(TEXT("전투 시작"));

    // Kill feedback is slightly larger/longer by default, but both profiles are editable in WBP defaults.
    KillMarkerAnimation.FadeInDuration = 0.03f;
    KillMarkerAnimation.HoldDuration = 0.08f;
    KillMarkerAnimation.FadeOutDuration = 0.20f;
    KillMarkerAnimation.ScaleUpDuration = 0.10f;
    KillMarkerAnimation.StartScale = 0.78f;
    KillMarkerAnimation.EndScale = 1.12f;
}

void UAscInGameHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (!OwningAscPC)
    {
        OwningAscPC = Cast<AAscenduntPlayerController>(GetOwningPlayer());
    }

    BindMatchGameState();

    if (MissionObjectiveWidget)
    {
        // MissionObjectiveWidget owns mission/objective presentation itself.
        // It intentionally does not route through Txt_CenterMessage/ShowCenterMessage().
        MissionObjectiveWidget->TryAutoBindMissionDirector();
    }

    if (InteractionRoot)
    {
        InteractionRoot->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (Progress_Interaction)
    {
        Progress_Interaction->SetPercent(0.0f);
        Progress_Interaction->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (Txt_InteractionProgress)
    {
        Txt_InteractionProgress->SetText(FText::AsPercent(0.0f));
        Txt_InteractionProgress->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (DownedRoot)
    {
        DownedRoot->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (Txt_DownedRemaining)
    {
        Txt_DownedRemaining->SetText(FText::FromString(TEXT("0.0")));
    }

    RefreshAll();

    if (Txt_CenterMessage)
    {
        Txt_CenterMessage->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (Txt_PhaseMessage && Txt_PhaseMessage->GetText().IsEmpty())
    {
        Txt_PhaseMessage->SetVisibility(ESlateVisibility::Collapsed);
    }

    ResetHitMarkerPresentation();
}

void UAscInGameHUDWidget::NativeDestruct()
{
    ResetHitMarkerPresentation();
    ClearCenterMessage();
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DownedHUDRefreshTimerHandle);
    }
    ClearHeroHUDPanel();
    UnbindUltimateAttributes();
    UnbindObservedPawn();
    UnbindMatchGameState();

    Super::NativeDestruct();
}

void UAscInGameHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (UImage* ActiveImage = ActiveHitMarkerImage.Get())
    {
        const FAscHitMarkerAnimationSettings& ActiveSettings =
            bActiveHitMarkerIsHeadshot ? HeadshotMarkerAnimation : HitMarkerAnimation;
        UpdateMarkerAnimation(ActiveImage, HitMarkerRuntime, ActiveSettings, InDeltaTime);
        if (!HitMarkerRuntime.bPlaying)
        {
            ActiveHitMarkerImage.Reset();
        }
    }

    UpdateMarkerAnimation(Img_KillMarker, KillMarkerRuntime, KillMarkerAnimation, InDeltaTime);
}

void UAscInGameHUDWidget::InitializeInGameHUD(AAscenduntPlayerController* InOwningAscPC)
{
    OwningAscPC = InOwningAscPC ? InOwningAscPC : Cast<AAscenduntPlayerController>(GetOwningPlayer());
    BindUltimateAttributes();

    if (ActiveHeroHUDPanel)
    {
        ActiveHeroHUDPanel->InitializeHeroHUDPanel(OwningAscPC);
    }

    if (MissionObjectiveWidget)
    {
        MissionObjectiveWidget->TryAutoBindMissionDirector();
    }

    RefreshAll();
}

void UAscInGameHUDWidget::NotifyOwningPawnChanged(APawn* NewPawn)
{
    // Pawn replacement is pushed from PlayerController::OnPossess / OnRep_Pawn.
    // All health, weapon and ammo updates after this point are delegate-driven.
    BindObservedPawn(NewPawn);
    BindUltimateAttributes();

    if (ActiveHeroHUDPanel)
    {
        ActiveHeroHUDPanel->NotifyOwningPawnChanged(NewPawn);
    }

    // Gives the HUD another deterministic bind opportunity during possession or
    // respawn without reintroducing per-frame GameState polling.
    BindMatchGameState();
    HandleMatchGameStateChanged();
}

void UAscInGameHUDWidget::SetHeroHUDPanelClass(TSubclassOf<UAscHeroHUDPanelWidget> NewPanelClass)
{
    const bool bClassAlreadySelected = ActiveHeroHUDPanelClass == NewPanelClass;
    if (bClassAlreadySelected && (!NewPanelClass || IsValid(ActiveHeroHUDPanel)))
    {
        return;
    }

    ClearHeroHUDPanel();
    ActiveHeroHUDPanelClass = NewPanelClass;

    if (!NewPanelClass || !Overlay_HeroHUDPanel)
    {
        return;
    }

    ActiveHeroHUDPanel = CreateWidget<UAscHeroHUDPanelWidget>(
        GetOwningPlayer(),
        NewPanelClass);

    if (!ActiveHeroHUDPanel)
    {
        ActiveHeroHUDPanelClass = nullptr;
        return;
    }

    ActiveHeroHUDPanel->InitializeHeroHUDPanel(OwningAscPC);

    if (UOverlaySlot* HeroPanelSlot =
        Overlay_HeroHUDPanel->AddChildToOverlay(ActiveHeroHUDPanel))
    {
        HeroPanelSlot->SetHorizontalAlignment(HAlign_Fill);
        HeroPanelSlot->SetVerticalAlignment(VAlign_Fill);
        HeroPanelSlot->SetPadding(FMargin(0.0f));
    }
}

void UAscInGameHUDWidget::ShowCenterMessage(const FText& Message, float DurationSeconds)
{
    UWorld* World = GetWorld();
    if (World)
    {
        World->GetTimerManager().ClearTimer(CenterMessageTimerHandle);
    }

    if (!Txt_CenterMessage)
    {
        return;
    }

    Txt_CenterMessage->SetText(Message);
    Txt_CenterMessage->SetVisibility(
        Message.IsEmpty()
            ? ESlateVisibility::Collapsed
            : ESlateVisibility::HitTestInvisible);

    if (!Message.IsEmpty() && DurationSeconds > 0.0f && World)
    {
        World->GetTimerManager().SetTimer(
            CenterMessageTimerHandle,
            this,
            &ThisClass::ClearCenterMessage,
            DurationSeconds,
            false);
    }
}

void UAscInGameHUDWidget::ClearCenterMessage()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(CenterMessageTimerHandle);
    }

    if (Txt_CenterMessage)
    {
        Txt_CenterMessage->SetText(FText::GetEmpty());
        Txt_CenterMessage->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UAscInGameHUDWidget::SetPhaseMessageText(const FText& Message)
{
    if (!Txt_PhaseMessage)
    {
        return;
    }

    Txt_PhaseMessage->SetText(Message);
    Txt_PhaseMessage->SetVisibility(Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UAscInGameHUDWidget::SetPortraitTexture(UTexture2D* InPortrait)
{
    if (!Img_Portrait)
    {
        return;
    }

    if (InPortrait)
    {
        Img_Portrait->SetBrushFromTexture(InPortrait, true);
        Img_Portrait->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    else
    {
        Img_Portrait->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UAscInGameHUDWidget::ShowHitMarker(bool bHeadshot, bool bKilled)
{
    UImage* SelectedHitImage = bHeadshot ? Img_HeadshotMarker : Img_HitMarker;
    bActiveHitMarkerIsHeadshot = bHeadshot;

    // Hit and headshot share the same layer. A new confirmed hit restarts that layer immediately.
    if (Img_HitMarker && Img_HitMarker != SelectedHitImage)
    {
        Img_HitMarker->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (Img_HeadshotMarker && Img_HeadshotMarker != SelectedHitImage)
    {
        Img_HeadshotMarker->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (SelectedHitImage)
    {
        ActiveHitMarkerImage = SelectedHitImage;
        StartMarkerAnimation(SelectedHitImage, HitMarkerRuntime);
    }

    // Kill is deliberately independent. On a killing shot this starts in the same local call
    // as the body/headshot marker, so both layers animate simultaneously.
    if (bKilled && Img_KillMarker)
    {
        StartMarkerAnimation(Img_KillMarker, KillMarkerRuntime);
    }
}

void UAscInGameHUDWidget::StartMarkerAnimation(UImage* Image, FAscHitMarkerRuntimeState& State)
{
    if (!Image)
    {
        State = FAscHitMarkerRuntimeState();
        return;
    }

    State.Elapsed = 0.0f;
    State.bPlaying = true;

    Image->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
    Image->SetRenderOpacity(0.0f);
    Image->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UAscInGameHUDWidget::UpdateMarkerAnimation(
    UImage* Image,
    FAscHitMarkerRuntimeState& State,
    const FAscHitMarkerAnimationSettings& Settings,
    float DeltaTime)
{
    if (!Image || !State.bPlaying)
    {
        return;
    }

    State.Elapsed += FMath::Max(0.0f, DeltaTime);

    const float FadeIn = FMath::Max(0.0f, Settings.FadeInDuration);
    const float Hold = FMath::Max(0.0f, Settings.HoldDuration);
    const float FadeOut = FMath::Max(0.0f, Settings.FadeOutDuration);
    const float TotalDuration = FadeIn + Hold + FadeOut;

    float Opacity = 1.0f;
    if (FadeIn > KINDA_SMALL_NUMBER && State.Elapsed < FadeIn)
    {
        Opacity = FMath::Clamp(State.Elapsed / FadeIn, 0.0f, 1.0f);
    }
    else if (State.Elapsed >= FadeIn + Hold)
    {
        if (FadeOut > KINDA_SMALL_NUMBER)
        {
            Opacity = 1.0f - FMath::Clamp((State.Elapsed - FadeIn - Hold) / FadeOut, 0.0f, 1.0f);
        }
        else
        {
            Opacity = 0.0f;
        }
    }

    const float ScaleDuration = FMath::Max(0.0f, Settings.ScaleUpDuration);
    const float ScaleAlpha = ScaleDuration > KINDA_SMALL_NUMBER
        ? FMath::Clamp(State.Elapsed / ScaleDuration, 0.0f, 1.0f)
        : 1.0f;
    const float Scale = FMath::Lerp(Settings.StartScale, Settings.EndScale, ScaleAlpha);

    Image->SetRenderOpacity(Opacity);
    Image->SetRenderScale(FVector2D(Scale, Scale));

    if (TotalDuration <= KINDA_SMALL_NUMBER || State.Elapsed >= TotalDuration)
    {
        State.bPlaying = false;
        State.Elapsed = 0.0f;
        Image->SetRenderOpacity(0.0f);
        Image->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UAscInGameHUDWidget::ResetHitMarkerPresentation()
{
    HitMarkerRuntime = FAscHitMarkerRuntimeState();
    KillMarkerRuntime = FAscHitMarkerRuntimeState();
    ActiveHitMarkerImage.Reset();
    bActiveHitMarkerIsHeadshot = false;

    const auto ResetImage = [](UImage* Image)
    {
        if (!Image)
        {
            return;
        }

        Image->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
        Image->SetRenderOpacity(0.0f);
        Image->SetRenderScale(FVector2D(1.0f, 1.0f));
        Image->SetVisibility(ESlateVisibility::Collapsed);
    };

    ResetImage(Img_HitMarker);
    ResetImage(Img_HeadshotMarker);
    ResetImage(Img_KillMarker);
}

void UAscInGameHUDWidget::BeginPvEMissionExitTransition(bool bMissionSuccess, float TravelDelaySeconds)
{
    BP_OnPvEMissionExitTransition(
        bMissionSuccess,
        FMath::Max(0.0f, TravelDelaySeconds));
}

void UAscInGameHUDWidget::HandleAmmoChanged(int CurrentAmmo, int MaxAmmo)
{
    if (Txt_Ammo)
    {
        Txt_Ammo->SetText(FText::Format(
            FText::FromString(TEXT("{0} / {1}")),
            FText::AsNumber(CurrentAmmo),
            FText::AsNumber(MaxAmmo)));
    }
}

void UAscInGameHUDWidget::HandleMatchGameStateChanged()
{
    RefreshMatchStateText();

    if (bMatchResultUIShown)
    {
        return;
    }

    AMatchGameState* MatchGS = CachedMatchGameState.Get();
    if (!MatchGS)
    {
        MatchGS = GetWorld()
            ? GetWorld()->GetGameState<AMatchGameState>()
            : nullptr;

        CachedMatchGameState = MatchGS;
    }

    if (!MatchGS)
    {
        return;
    }

    const FAscMatchResultState& Result =
        MatchGS->GetMatchResult();

    if (!Result.bFinalized)
    {
        return;
    }

    APlayerController* OwningPC = GetOwningPlayer();
    AAscenduntPlayerState* LocalPS = OwningPC
        ? OwningPC->GetPlayerState<AAscenduntPlayerState>()
        : nullptr;

    if (!LocalPS)
    {
        return;
    }

    const EAscMatchResult LocalResult =
        MatchGS->GetTeamMatchResult(LocalPS->GetTeamId());

    ShowMatchResultUI(LocalResult);
}

void UAscInGameHUDWidget::HandleCurrentWeaponChanged(AAscenduntWeapon* NewWeapon, AAscenduntWeapon* PrevWeapon)
{
    BindObservedWeapon(NewWeapon);
}

void UAscInGameHUDWidget::BindMatchGameState()
{
    AMatchGameState* MatchGS = GetWorld() ? GetWorld()->GetGameState<AMatchGameState>() : nullptr;
    if (CachedMatchGameState.Get() == MatchGS)
    {
        if (MapIntroWidget)
        {
            MapIntroWidget->InitializeMapIntro(MatchGS);
        }
        return;
    }

    UnbindMatchGameState();

    CachedMatchGameState = MatchGS;
    if (MatchGS)
    {
        LastObservedPhase = MatchGS->GetMatchPhase();
        MatchGS->OnMatchGameStateChanged.RemoveAll(this);
        MatchGS->OnMatchGameStateChanged.AddUObject(this, &ThisClass::HandleMatchGameStateChanged);
    }

    if (MapIntroWidget)
    {
        MapIntroWidget->InitializeMapIntro(MatchGS);
    }
}

void UAscInGameHUDWidget::UnbindMatchGameState()
{
    if (AMatchGameState* MatchGS = CachedMatchGameState.Get())
    {
        MatchGS->OnMatchGameStateChanged.RemoveAll(this);
    }

    if (MapIntroWidget)
    {
        MapIntroWidget->InitializeMapIntro(nullptr);
    }

    CachedMatchGameState.Reset();
}

void UAscInGameHUDWidget::HandleFocusedInteractionChanged(AActor* TargetActor, FText PromptText, EAscInteractionInputMode InputMode, float HoldDuration)
{
    bool bCanShowInteraction = false;

    AHeroCharacter* Interactor = Cast<AHeroCharacter>(ObservedPawn.Get());
    if (IsValid(TargetActor)
        && IsValid(Interactor)
        && TargetActor->GetClass()->ImplementsInterface(UAscInteractable::StaticClass()))
    {
        // InteractionComponent already validates CanInteract while choosing focus.
        // Re-check at the final HUD boundary so stale focus/state changes can never show a prompt.
        bCanShowInteraction = IAscInteractable::Execute_CanInteract(TargetActor, Interactor);
    }

    const bool bHasTarget = bCanShowInteraction;
    bFocusedInteractionRequiresHold = bHasTarget && InputMode == EAscInteractionInputMode::Hold;

    if (InteractionRoot)
    {
        InteractionRoot->SetVisibility(
            bHasTarget ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }

    if (Txt_InteractionPrompt)
    {
        if (!bHasTarget)
        {
            Txt_InteractionPrompt->SetText(FText::GetEmpty());
            Txt_InteractionPrompt->SetVisibility(ESlateVisibility::Collapsed);
        }
        else
        {
            const FText EffectivePrompt = PromptText.IsEmpty()
                ? FText::FromString(TEXT("상호작용"))
                : PromptText;
            const FText& Format = bFocusedInteractionRequiresHold
                ? InteractionHoldFormat
                : InteractionTapFormat;
            Txt_InteractionPrompt->SetText(FText::Format(Format, EffectivePrompt));
            Txt_InteractionPrompt->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
    }

    if (Progress_Interaction)
    {
        Progress_Interaction->SetPercent(0.0f);
        Progress_Interaction->SetVisibility(
            bFocusedInteractionRequiresHold
                ? ESlateVisibility::HitTestInvisible
                : ESlateVisibility::Collapsed);
    }
    if (Txt_InteractionProgress)
    {
        Txt_InteractionProgress->SetText(FText::AsPercent(0.0f));
        Txt_InteractionProgress->SetVisibility(
            bFocusedInteractionRequiresHold
                ? ESlateVisibility::HitTestInvisible
                : ESlateVisibility::Collapsed);
    }
}

void UAscInGameHUDWidget::HandleInteractionProgressChanged(float Progress, bool bIsInteracting)
{
    const float ClampedProgress = FMath::Clamp(Progress, 0.0f, 1.0f);

    if (Progress_Interaction)
    {
        Progress_Interaction->SetPercent(bFocusedInteractionRequiresHold ? ClampedProgress : 0.0f);
        Progress_Interaction->SetVisibility(
            bFocusedInteractionRequiresHold
                ? ESlateVisibility::HitTestInvisible
                : ESlateVisibility::Collapsed);
    }

    if (Txt_InteractionProgress)
    {
        Txt_InteractionProgress->SetText(FText::AsPercent(
            bFocusedInteractionRequiresHold ? ClampedProgress : 0.0f));
        Txt_InteractionProgress->SetVisibility(
            bFocusedInteractionRequiresHold
                ? ESlateVisibility::HitTestInvisible
                : ESlateVisibility::Collapsed);
    }
}

void UAscInGameHUDWidget::HandleObservedDownedStateChanged(bool bIsDowned)
{
    if (DownedRoot)
    {
        DownedRoot->SetVisibility(
            bIsDowned ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    World->GetTimerManager().ClearTimer(DownedHUDRefreshTimerHandle);

    if (bIsDowned)
    {
        RefreshDownedHUD();
        World->GetTimerManager().SetTimer(
            DownedHUDRefreshTimerHandle,
            this,
            &ThisClass::RefreshDownedHUD,
            0.1f,
            true);
    }
    else if (Txt_DownedRemaining)
    {
        Txt_DownedRemaining->SetText(FText::FromString(TEXT("0.0")));
    }
}

void UAscInGameHUDWidget::RefreshDownedHUD()
{
    const UAscPvEDownedComponent* Downed = ObservedDowned.Get();
    if (!Downed || !Downed->IsDowned())
    {
        HandleObservedDownedStateChanged(false);
        return;
    }

    if (Txt_DownedRemaining)
    {
        const float Remaining = Downed->GetBleedoutRemainingTime();
        Txt_DownedRemaining->SetText(
            FText::FromString(FString::Printf(TEXT("%.1f"), FMath::Max(0.0f, Remaining))));
    }
}

void UAscInGameHUDWidget::BindObservedPawn(APawn* NewPawn)
{
    if (ObservedPawn.Get() == NewPawn)
    {
        // A repeated possession notification is still a valid request to sync the
        // initial values, but delegate bindings do not need to be rebuilt.
        if (PlayerHealthPanel)
        {
            PlayerHealthPanel->SetObservedPawn(NewPawn);
        }
        if (UAscInteractionComponent* Interaction = ObservedInteraction.Get())
        {
            const FAscInteractionOption Option = Interaction->GetFocusedInteractionOption();
            HandleFocusedInteractionChanged(Interaction->GetFocusedInteractable(), Option.PromptText, Option.InputMode, Option.HoldDuration);
        }
        if (UAscPvEDownedComponent* Downed = ObservedDowned.Get())
        {
            HandleObservedDownedStateChanged(Downed->IsDowned());
        }
        RefreshAmmo();
        RefreshPortrait();
        return;
    }

    UnbindObservedPawn();


    ObservedPawn = NewPawn;

    if (PlayerHealthPanel)
    {
        PlayerHealthPanel->SetObservedPawn(NewPawn);
    }

    if (AHeroCharacter* Hero = Cast<AHeroCharacter>(NewPawn))
    {
        BindObservedInteraction(Hero->GetInteractionComponent());
        BindObservedDowned(Hero->GetPvEDownedComponent());

        ObservedWeaponEquipment = Hero->GetWeaponEquipmentComponent();
        if (UWeaponEquipmentComponent* Equipment = ObservedWeaponEquipment.Get())
        {
            Equipment->OnCurrentWeaponChanged.RemoveAll(this);
            Equipment->OnCurrentWeaponChanged.AddUObject(this, &ThisClass::HandleCurrentWeaponChanged);
            BindObservedWeapon(Equipment->GetCurrentWeapon());
        }
    }

    RefreshAmmo();
    RefreshPortrait();
}

void UAscInGameHUDWidget::UnbindObservedPawn()
{
    UnbindObservedWeapon();
    UnbindObservedInteraction();
    UnbindObservedDowned();

    if (PlayerHealthPanel)
    {
        PlayerHealthPanel->ClearObservedPawn();
    }


    if (UWeaponEquipmentComponent* Equipment = ObservedWeaponEquipment.Get())
    {
        Equipment->OnCurrentWeaponChanged.RemoveAll(this);
    }

    ObservedPawn.Reset();
    ObservedWeaponEquipment.Reset();
}

void UAscInGameHUDWidget::BindObservedInteraction(UAscInteractionComponent* NewInteraction)
{
    UnbindObservedInteraction();

    ObservedInteraction = NewInteraction;
    if (!NewInteraction)
    {
        HandleFocusedInteractionChanged(nullptr, FText::GetEmpty(), EAscInteractionInputMode::Pressed, 0.0f);
        HandleInteractionProgressChanged(0.0f, false);
        return;
    }

    NewInteraction->OnFocusedInteractionChanged.RemoveDynamic(
        this, &ThisClass::HandleFocusedInteractionChanged);
    NewInteraction->OnFocusedInteractionChanged.AddDynamic(
        this, &ThisClass::HandleFocusedInteractionChanged);

    NewInteraction->OnInteractionProgressChanged.RemoveDynamic(
        this, &ThisClass::HandleInteractionProgressChanged);
    NewInteraction->OnInteractionProgressChanged.AddDynamic(
        this, &ThisClass::HandleInteractionProgressChanged);

    const FAscInteractionOption Option = NewInteraction->GetFocusedInteractionOption();
    HandleFocusedInteractionChanged(
        NewInteraction->GetFocusedInteractable(),
        Option.PromptText,
        Option.InputMode,
        Option.HoldDuration);
    HandleInteractionProgressChanged(0.0f, false);
}

void UAscInGameHUDWidget::UnbindObservedInteraction()
{
    if (UAscInteractionComponent* Interaction = ObservedInteraction.Get())
    {
        Interaction->OnFocusedInteractionChanged.RemoveDynamic(
            this, &ThisClass::HandleFocusedInteractionChanged);
        Interaction->OnInteractionProgressChanged.RemoveDynamic(
            this, &ThisClass::HandleInteractionProgressChanged);
    }

    ObservedInteraction.Reset();
    HandleFocusedInteractionChanged(nullptr, FText::GetEmpty(), EAscInteractionInputMode::Pressed, 0.0f);
    HandleInteractionProgressChanged(0.0f, false);
}

void UAscInGameHUDWidget::BindObservedDowned(UAscPvEDownedComponent* NewDowned)
{
    UnbindObservedDowned();

    ObservedDowned = NewDowned;
    if (!NewDowned)
    {
        HandleObservedDownedStateChanged(false);
        return;
    }

    NewDowned->OnDownedStateChanged.RemoveAll(this);
    NewDowned->OnDownedStateChanged.AddUObject(this, &ThisClass::HandleObservedDownedStateChanged);
    HandleObservedDownedStateChanged(NewDowned->IsDowned());
}

void UAscInGameHUDWidget::UnbindObservedDowned()
{
    if (UAscPvEDownedComponent* Downed = ObservedDowned.Get())
    {
        Downed->OnDownedStateChanged.RemoveAll(this);
    }

    ObservedDowned.Reset();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DownedHUDRefreshTimerHandle);
    }

    if (DownedRoot)
    {
        DownedRoot->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (Txt_DownedRemaining)
    {
        Txt_DownedRemaining->SetText(FText::FromString(TEXT("0.0")));
    }
}

void UAscInGameHUDWidget::BindObservedWeapon(AAscenduntWeapon* NewWeapon)
{
    UnbindObservedWeapon();

    ObservedWeapon = NewWeapon;
    BindAmmoAttributes();
    if (NewWeapon)
    {


    }

    RefreshAmmo();
}

void UAscInGameHUDWidget::UnbindObservedWeapon()
{
    UnbindAmmoAttributes();
    if (AAscenduntWeapon* Weapon = ObservedWeapon.Get())
    {

    }

    ObservedWeapon.Reset();
}

void UAscInGameHUDWidget::RefreshAll()
{
    BindMatchGameState();

    if (!OwningAscPC)
    {
        OwningAscPC = Cast<AAscenduntPlayerController>(GetOwningPlayer());
    }

    BindUltimateAttributes();

    APawn* CurrentPawn = OwningAscPC ? OwningAscPC->GetPawn() : nullptr;
    if (ObservedPawn.Get() != CurrentPawn)
    {
        BindObservedPawn(CurrentPawn);
    }
    else
    {
        if (PlayerHealthPanel)
        {
            PlayerHealthPanel->SetObservedPawn(CurrentPawn);
        }
        RefreshAmmo();
        RefreshPortrait();
    }

    HandleMatchGameStateChanged();
}


void UAscInGameHUDWidget::BindUltimateAttributes()
{
    UAscenduntAbilitySystemComponent* NewASC = nullptr;
    if (OwningAscPC)
    {
        if (AAscenduntPlayerState* PS = OwningAscPC->GetPlayerState<AAscenduntPlayerState>())
        {
            NewASC = PS->GetAscenduntAbilitySystemComponent();
        }
    }

    if (BoundUltimateASC.Get() == NewASC)
    {
        RefreshUltimateUI();
        return;
    }

    UnbindUltimateAttributes();
    BoundUltimateASC = NewASC;

    if (NewASC)
    {
        UltimateChargeChangedHandle = NewASC->GetGameplayAttributeValueChangeDelegate(
            UAscenduntUltimateSet::GetUltimateChargeAttribute()).AddUObject(
                this, &ThisClass::HandleUltimateChargeChanged);

        MaxUltimateChargeChangedHandle = NewASC->GetGameplayAttributeValueChangeDelegate(
            UAscenduntUltimateSet::GetMaxUltimateChargeAttribute()).AddUObject(
                this, &ThisClass::HandleMaxUltimateChargeChanged);
    }

    RefreshUltimateUI();
}

void UAscInGameHUDWidget::UnbindUltimateAttributes()
{
    if (UAbilitySystemComponent* ASC = BoundUltimateASC.Get())
    {
        if (UltimateChargeChangedHandle.IsValid())
        {
            ASC->GetGameplayAttributeValueChangeDelegate(
                UAscenduntUltimateSet::GetUltimateChargeAttribute()).Remove(UltimateChargeChangedHandle);
        }
        if (MaxUltimateChargeChangedHandle.IsValid())
        {
            ASC->GetGameplayAttributeValueChangeDelegate(
                UAscenduntUltimateSet::GetMaxUltimateChargeAttribute()).Remove(MaxUltimateChargeChangedHandle);
        }
    }

    UltimateChargeChangedHandle.Reset();
    MaxUltimateChargeChangedHandle.Reset();
    BoundUltimateASC.Reset();
}

void UAscInGameHUDWidget::HandleUltimateChargeChanged(const FOnAttributeChangeData& ChangeData)
{
    RefreshUltimateUI();
}

void UAscInGameHUDWidget::HandleMaxUltimateChargeChanged(const FOnAttributeChangeData& ChangeData)
{
    RefreshUltimateUI();
}

void UAscInGameHUDWidget::RefreshUltimateUI()
{
    UAbilitySystemComponent* ASC = BoundUltimateASC.Get();
    if (!ASC)
    {
        if (Progress_UltimateCharge)
        {
            Progress_UltimateCharge->SetPercent(0.f);
            Progress_UltimateCharge->SetVisibility(ESlateVisibility::Collapsed);
        }
        if (Txt_UltimateCharge)
        {
            Txt_UltimateCharge->SetText(FText::AsNumber(0));
            Txt_UltimateCharge->SetVisibility(ESlateVisibility::Collapsed);
        }
        if (Img_UltimateReady)
        {
            Img_UltimateReady->SetVisibility(ESlateVisibility::Collapsed);
        }
        return;
    }

    const float Charge = FMath::Max(0.f, ASC->GetNumericAttribute(
        UAscenduntUltimateSet::GetUltimateChargeAttribute()));
    const float MaxCharge = FMath::Max(1.f, ASC->GetNumericAttribute(
        UAscenduntUltimateSet::GetMaxUltimateChargeAttribute()));
    const float Percent = FMath::Clamp(Charge / MaxCharge, 0.f, 1.f);
    const bool bReady = Charge >= MaxCharge - KINDA_SMALL_NUMBER;

    if (Progress_UltimateCharge)
    {
        Progress_UltimateCharge->SetPercent(Percent);
        Progress_UltimateCharge->SetVisibility(
            bReady ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    }

    if (Txt_UltimateCharge)
    {
        Txt_UltimateCharge->SetText(FText::AsNumber(FMath::RoundToInt(Percent * 100.f)));
        Txt_UltimateCharge->SetVisibility(
            bReady ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    }

    if (Img_UltimateReady)
    {
        Img_UltimateReady->SetVisibility(
            bReady ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
}

void UAscInGameHUDWidget::RefreshAmmo()
{
    BindAmmoAttributes();
    AAscenduntWeapon* Weapon = ObservedWeapon.Get();
    if (!Txt_Ammo)
    {
        return;
    }

    if (!Weapon)
    {
        Txt_Ammo->SetText(FText::FromString(TEXT("-- / --")));
        return;
    }

    const int32 CurrentAmmo = Weapon->GetCurrentAmmoInMag();
    const int32 MaxAmmo = Weapon->GetMagSize();
    Txt_Ammo->SetText(FText::Format(
        FText::FromString(TEXT("{0} / {1}")),
        FText::AsNumber(CurrentAmmo),
        FText::AsNumber(MaxAmmo)));
}

void UAscInGameHUDWidget::RefreshPortrait()
{
    if (!Img_Portrait)
    {
        return;
    }

    const APawn* CurrentPawn = ObservedPawn.Get();
    if (!CurrentPawn)
    {
        SetPortraitTexture(nullptr);
        return;
    }

    const UAscHeroRosterData* RosterData = OwningAscPC
        ? OwningAscPC->GetHeroRosterData()
        : nullptr;

    if (!RosterData)
    {
        SetPortraitTexture(nullptr);
        return;
    }

    // The portrait follows the currently possessed Pawn class. This avoids briefly
    // showing stale replicated hero ids while a replacement Pawn is being created.
    const FAscHeroDefinition* HeroDefinition =
        RosterData->FindHeroDefinitionByPawnClass(CurrentPawn->GetClass());

    SetPortraitTexture(HeroDefinition ? HeroDefinition->Portrait : nullptr);
}

void UAscInGameHUDWidget::RefreshMatchStateText()
{
    AMatchGameState* MatchGS = CachedMatchGameState.Get();
    if (!MatchGS)
    {
        MatchGS = GetWorld() ? GetWorld()->GetGameState<AMatchGameState>() : nullptr;
        CachedMatchGameState = MatchGS;
    }

    if (!MatchGS)
    {
        if (MapIntroWidget)
        {
            MapIntroWidget->SetVisibility(ESlateVisibility::Collapsed);
        }

        if (Txt_PhaseCountdown)
        {
            Txt_PhaseCountdown->SetVisibility(ESlateVisibility::Collapsed);
        }
        return;
    }

    const EAscMatchPhase CurrentPhase = MatchGS->GetMatchPhase();
    const FAscMatchClockState& ClockState = MatchGS->GetClockState();

    if (MapIntroWidget)
    {
        MapIntroWidget->SetVisibility(
            CurrentPhase == EAscMatchPhase::MapIntro
                ? ESlateVisibility::HitTestInvisible
                : ESlateVisibility::Collapsed);
    }

    if (CurrentPhase != LastObservedPhase)
    {
        if (bShowPlayingStartMessage && CurrentPhase == EAscMatchPhase::Playing &&
            LastObservedPhase == EAscMatchPhase::PreRoundWaiting)
        {
            ShowCenterMessage(PlayingStartMessage, PlayingStartMessageDuration);
        }

        LastObservedPhase = CurrentPhase;
    }

    switch (CurrentPhase)
    {
    case EAscMatchPhase::PreRoundWaiting:
    {
        const int32 RemainingSeconds =
            FMath::Max(0, ClockState.PhaseRemainingSeconds);

        Txt_PhaseCountdown->SetText(FText::AsNumber(RemainingSeconds));
        Txt_PhaseCountdown->SetVisibility(
            ESlateVisibility::HitTestInvisible);
        break;
    }

    case EAscMatchPhase::Playing:
    {
        const int32 RemainingSeconds =
            FMath::Max(0, ClockState.RemainingTimeSeconds);

        const int32 Minutes = RemainingSeconds / 60;
        const int32 Seconds = RemainingSeconds % 60;

        Txt_PhaseCountdown->SetText(
            FText::FromString(
                FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds)));

        Txt_PhaseCountdown->SetVisibility(
            ESlateVisibility::HitTestInvisible);
        break;
    }

    default:
        Txt_PhaseCountdown->SetVisibility(
            ESlateVisibility::Collapsed);
        break;
    }
}

void UAscInGameHUDWidget::ClearHeroHUDPanel()
{
    if (ActiveHeroHUDPanel)
    {
        ActiveHeroHUDPanel->NotifyOwningPawnChanged(nullptr);
        ActiveHeroHUDPanel->RemoveFromParent();
    }

    ActiveHeroHUDPanel = nullptr;
    ActiveHeroHUDPanelClass = nullptr;
}

void UAscInGameHUDWidget::ShowMatchResultUI(EAscMatchResult MatchResult)
{
    if (bMatchResultUIShown)
    {
        return;
    }

    FText ResultText;

    switch (MatchResult)
    {
    case EAscMatchResult::Victory:
        ResultText = FText::FromString(TEXT("승리"));
        break;

    case EAscMatchResult::Defeat:
        ResultText = FText::FromString(TEXT("패배"));
        break;

    case EAscMatchResult::Draw:
        ResultText = FText::FromString(TEXT("무승부"));
        break;

    default:
        return;
    }

    bMatchResultUIShown = true;

    // 체력, 탄약, 초상화, 시간 등 기존 HUD 전체 숨김
    if (GameplayHUDRoot)
    {
        GameplayHUDRoot->SetVisibility(
            ESlateVisibility::Collapsed);
    }

    // 중앙 결과 메시지만 표시
    ShowCenterMessage(ResultText, 3.0f);
}

void UAscInGameHUDWidget::BindAmmoAttributes()
{
	AAscenduntWeapon* Weapon = ObservedWeapon.Get();
	UAscenduntAbilitySystemComponent* ASC = Weapon ? Weapon->GetCachedAscASC() : nullptr;
	if (BoundAmmoASC.Get() == ASC)
	{
		return;
	}

	UnbindAmmoAttributes();
	BoundAmmoASC = ASC;
	if (ASC)
	{
		AmmoAttributeHandle = ASC->GetGameplayAttributeValueChangeDelegate(UAscenduntWeaponSet::GetCurrentAmmoAttribute())
		                          .AddUObject(this, &ThisClass::HandleAmmoAttributeChanged);
		MagAttributeHandle =
		    ASC->GetGameplayAttributeValueChangeDelegate(UAscenduntWeaponSet::GetMagSizeAttribute()).AddUObject(this, &ThisClass::HandleAmmoAttributeChanged);
	}
}

void UAscInGameHUDWidget::UnbindAmmoAttributes()
{
	if (UAscenduntAbilitySystemComponent* ASC = BoundAmmoASC.Get())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UAscenduntWeaponSet::GetCurrentAmmoAttribute()).Remove(AmmoAttributeHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(UAscenduntWeaponSet::GetMagSizeAttribute()).Remove(MagAttributeHandle);
	}

	BoundAmmoASC.Reset();
	AmmoAttributeHandle.Reset();
	MagAttributeHandle.Reset();
}

void UAscInGameHUDWidget::HandleAmmoAttributeChanged(const FOnAttributeChangeData& Data)
{
	RefreshAmmo();
}
