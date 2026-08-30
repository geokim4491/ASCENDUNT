#include "AscenduntPlayerController.h"

#include "AscenduntPlayerState.h"
#include "ASCENDUNT/Player/AscTargetAwarenessComponent.h"
#include "ASCENDUNT/GameModes/PVP/MatchGameState.h"
#include "ASCENDUNT/GameModes/PVP/MatchGameMode.h"
#include "ASCENDUNT/GameModes/PVE/AscPvEGameState.h"
#include "ASCENDUNT/GameModes/TestAndSettings/AscGameInstance.h"
#include "ASCENDUNT/HeroSelection/AscHeroSelectionAuthorityInterface.h"
#include "ASCENDUNT/HeroSelection/AscHeroRosterData.h"
#include "ASCENDUNT/UI/InGame/CharacterSelection/AscCharacterSelectionWidget.h"
#include "ASCENDUNT/UI/InGame/AscInGameHUDWidget.h"
#include "ASCENDUNT/UI/InGame/Scoreboard/AscScoreboardWidget.h"
#include "ASCENDUNT/UI/InGame/Hero/AscHeroHUDPanelWidget.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/Character/Hero/HeroInputComponent.h"
#include "ASCENDUNT/Camera/AscenduntCameraComponent.h"
#include "ASCENDUNT/Camera/AscenduntCameraMode_DeathSpectatorThirdPerson.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"
#include "GameFramework/GameModeBase.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GenericTeamAgentInterface.h"
#include "InputCoreTypes.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

AAscenduntPlayerController::AAscenduntPlayerController(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    TargetAwarenessComponent = CreateDefaultSubobject<UAscTargetAwarenessComponent>(TEXT("TargetAwarenessComponent"));
    DeathSpectatorCameraModeClass = UAscenduntCameraMode_DeathSpectatorThirdPerson::StaticClass();
}

void AAscenduntPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (IsLocalController())
    {
        SetupUIForCurrentMap();
    }
}



void AAscenduntPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (!InputComponent)
    {
        return;
    }

    FInputKeyBinding& SpectateBinding = InputComponent->BindKey(
        EKeys::SpaceBar,
        IE_Pressed,
        this,
        &ThisClass::HandleSpectateNextPressed);
    SpectateBinding.bConsumeInput = false;

    // HeroInputComponent and its Enhanced Input mappings are removed with the dead Pawn.
    // Bind raw mouse axes on the persistent PlayerController so spectator orbit remains usable.
    FInputAxisKeyBinding& SpectateYawBinding = InputComponent->BindAxisKey(
        EKeys::MouseX,
        this,
        &ThisClass::HandleDeathSpectateMouseYaw);
    SpectateYawBinding.bConsumeInput = false;

    FInputAxisKeyBinding& SpectatePitchBinding = InputComponent->BindAxisKey(
        EKeys::MouseY,
        this,
        &ThisClass::HandleDeathSpectateMousePitch);
    SpectatePitchBinding.bConsumeInput = false;

    // Do not consume H while alive; the possessed hero's GA_OpenCharacterSelection
    // still owns the normal respawn-room flow.
    FInputKeyBinding& DeadHeroSelectBinding = InputComponent->BindKey(
        EKeys::H,
        IE_Pressed,
        this,
        &ThisClass::HandleDeadHeroSelectionPressed);
    DeadHeroSelectBinding.bConsumeInput = false;

    // TAB scoreboard is presentation-only and belongs to the persistent PlayerController.
    // It remains available even when the hero Pawn/input component is gone during death spectating.
    FInputKeyBinding& ScoreboardPressedBinding = InputComponent->BindKey(
        EKeys::Tab,
        IE_Pressed,
        this,
        &ThisClass::HandleScoreboardPressed);
    ScoreboardPressedBinding.bConsumeInput = false;

    FInputKeyBinding& ScoreboardReleasedBinding = InputComponent->BindKey(
        EKeys::Tab,
        IE_Released,
        this,
        &ThisClass::HandleScoreboardReleased);
    ScoreboardReleasedBinding.bConsumeInput = false;
}

void AAscenduntPlayerController::PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel)
{
    if (IsLocalController())
    {
        HideHeroSelectionUI();
        ClearCurrentWidget();
    }

    Super::PreClientTravel(
        PendingURL,
        TravelType,
        bIsSeamlessTravel);
}

void AAscenduntPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    if (HasAuthority() && InPawn)
    {
        const APlayerState* PS = PlayerState;
        const IGenericTeamAgentInterface* PSTeam = Cast<IGenericTeamAgentInterface>(PS);
        IGenericTeamAgentInterface* PawnTeam = Cast<IGenericTeamAgentInterface>(InPawn);
        if (PSTeam && PawnTeam)
        {
            PawnTeam->SetGenericTeamId(PSTeam->GetGenericTeamId());
        }
    }

    if (IsLocalController() && IsMatchMap())
    {
        if (InPawn)
        {
            RefreshInGameHUDForPawn(InPawn);
        }
        else if (ActiveInGameHUDWidget)
        {
            // Keep the common HUD and current hero panel during the dead/no-pawn interval.
            ActiveInGameHUDWidget->NotifyOwningPawnChanged(nullptr);
        }
    }
}

void AAscenduntPlayerController::OnRep_Pawn()
{
    Super::OnRep_Pawn();

    if (IsLocalController() && IsMatchMap())
    {
        if (APawn* NewPawn = GetPawn())
        {
            RefreshInGameHUDForPawn(NewPawn);
        }
        else if (ActiveInGameHUDWidget)
        {
            // Keep the common HUD instance while waiting for respawn.
            ActiveInGameHUDWidget->NotifyOwningPawnChanged(nullptr);
        }
    }
}

void AAscenduntPlayerController::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();

    if (!IsLocalController() || !IsMatchMap())
    {
        return;
    }

    if (ActiveInGameHUDWidget)
    {
        ActiveInGameHUDWidget->InitializeInGameHUD(this);
        ActiveInGameHUDWidget->NotifyOwningPawnChanged(GetPawn());
    }
    else if (GetPawn())
    {
        RefreshInGameHUDForPawn(GetPawn());
    }
}

void AAscenduntPlayerController::PostSeamlessTravel()
{
    Super::PostSeamlessTravel();

    if (IsLocalController())
    {
        SetupUIForCurrentMap();
    }
}

void AAscenduntPlayerController::SetupUIForCurrentMap()
{
    ClearCurrentWidget();

    if (IsMainMenuMap())
    {
        ShowMainMenuUI();
    }
    else if (IsMatchMap())
    {
        ShowMatchUI();
    }
}

void AAscenduntPlayerController::ClearCurrentWidget()
{
    ClearScoreboardWidget();

    if (ActiveInGameHUDWidget)
    {
        ActiveInGameHUDWidget->NotifyOwningPawnChanged(nullptr);
    }

    if (ActiveRootWidget)
    {
        ActiveRootWidget->RemoveFromParent();
    }

    ActiveInGameHUDWidget = nullptr;
    ActiveRootWidget = nullptr;
}

void AAscenduntPlayerController::ShowMainMenuUI()
{
    if (!MainMenuWidgetClass)
    {
        return;
    }

    ActiveRootWidget = CreateWidget<UUserWidget>(this, MainMenuWidgetClass);
    if (!ActiveRootWidget)
    {
        return;
    }

    ActiveRootWidget->AddToViewport();
    ApplyInputMode_UIOnly();
}

void AAscenduntPlayerController::ShowMatchUI()
{
    RefreshInGameHUDForPawn(GetPawn());
    ApplyInputMode_GameOnly();
}

void AAscenduntPlayerController::EnsureScoreboardWidget()
{
    if (!IsLocalController() || !IsMatchMap() || ActiveScoreboardWidget || !ScoreboardWidgetClass)
    {
        return;
    }

    ActiveScoreboardWidget = CreateWidget<UAscScoreboardWidget>(this, ScoreboardWidgetClass);
    if (!ActiveScoreboardWidget)
    {
        return;
    }

    ActiveScoreboardWidget->InitializeScoreboard(this);
    ActiveScoreboardWidget->AddToViewport(50);
    ActiveScoreboardWidget->SetScoreboardVisible(false);
}

void AAscenduntPlayerController::ClearScoreboardWidget()
{
    if (ActiveScoreboardWidget)
    {
        ActiveScoreboardWidget->RemoveFromParent();
        ActiveScoreboardWidget = nullptr;
    }
}

void AAscenduntPlayerController::ShowScoreboard()
{
    if (!IsLocalController() || !IsMatchMap())
    {
        return;
    }

    EnsureScoreboardWidget();
    if (ActiveScoreboardWidget)
    {
        ActiveScoreboardWidget->SetScoreboardVisible(true);
    }
}

void AAscenduntPlayerController::HideScoreboard()
{
    if (ActiveScoreboardWidget)
    {
        ActiveScoreboardWidget->SetScoreboardVisible(false);
    }
}

void AAscenduntPlayerController::HandleScoreboardPressed()
{
    ShowScoreboard();
}

void AAscenduntPlayerController::HandleScoreboardReleased()
{
    HideScoreboard();
}

TSubclassOf<UAscHeroHUDPanelWidget> AAscenduntPlayerController::ResolveHeroHUDPanelClass(const APawn* Char) const
{
    if (!Char || !HeroRosterData)
    {
        return nullptr;
    }

    const FAscHeroDefinition* Definition =
        HeroRosterData->FindHeroDefinitionByPawnClass(Char->GetClass());

    return Definition ? Definition->HeroHUDPanelClass : nullptr;
}

void AAscenduntPlayerController::RefreshInGameHUDForPawn(APawn* NewPawn)
{
    if (!IsLocalController() || !IsMatchMap())
    {
        return;
    }

    // The common HUD is created once and remains alive across death, respawn,
    // and hero replacement. Only its optional hero-specific panel is exchanged.
    if (!ActiveInGameHUDWidget)
    {
        if (!MatchWidgetClass)
        {
            return;
        }

        ActiveInGameHUDWidget = CreateWidget<UAscInGameHUDWidget>(this, MatchWidgetClass);
        ActiveRootWidget = ActiveInGameHUDWidget;
        if (!ActiveInGameHUDWidget)
        {
            ActiveRootWidget = nullptr;
            return;
        }

        ActiveInGameHUDWidget->InitializeInGameHUD(this);
        ActiveInGameHUDWidget->AddToViewport();
    }

    // A null Pawn is the normal death/respawn interval. Keep the previously selected
    // hero panel instance, but detach it from gameplay data until the new Pawn arrives.
    if (NewPawn)
    {
        ActiveInGameHUDWidget->SetHeroHUDPanelClass(
            ResolveHeroHUDPanelClass(NewPawn));
    }

    ActiveInGameHUDWidget->NotifyOwningPawnChanged(NewPawn);
}

void AAscenduntPlayerController::ApplyInputMode_UIOnly()
{
    FInputModeUIOnly InputMode;
    if (ActiveHeroSelectionWidget)
    {
        InputMode.SetWidgetToFocus(ActiveHeroSelectionWidget->TakeWidget());
    }
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);
    SetShowMouseCursor(true);

    if (ActiveHeroSelectionWidget)
    {
        ActiveHeroSelectionWidget->SetKeyboardFocus();
    }
}

void AAscenduntPlayerController::ApplyInputMode_GameOnly()
{
    FInputModeGameOnly InputMode;
    SetInputMode(InputMode);
    SetShowMouseCursor(false);
}

FString AAscenduntPlayerController::GetCurrentMapName() const
{
    return GetWorld() ? GetWorld()->GetMapName() : FString();
}

bool AAscenduntPlayerController::IsMainMenuMap() const
{
    return GetCurrentMapName().Contains(TEXT("L_MainMenu"));
}

bool AAscenduntPlayerController::IsMatchMap() const
{
    const FString MapName = GetCurrentMapName();
    return !MapName.IsEmpty() && !IsMainMenuMap();
}

void AAscenduntPlayerController::SetTeam(uint8 NewTeam)
{
    Server_SetTeam(NewTeam);
}

void AAscenduntPlayerController::Server_SetTeam_Implementation(uint8 NewTeam)
{
    if (AAscenduntPlayerState* PS = GetPlayerState<AAscenduntPlayerState>())
    {
        PS->SetGenericTeamId(FGenericTeamId(NewTeam));
    }
}

void AAscenduntPlayerController::Server_RequestConfirmHero_Implementation(int32 HeroId, EAscHeroSelectionContext Context)
{
    AGameModeBase* CurrentGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AGameModeBase>() : nullptr;
    IAscHeroSelectionAuthorityInterface* HeroSelectionAuthority = Cast<IAscHeroSelectionAuthorityInterface>(CurrentGameMode);
    if (!HeroSelectionAuthority)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[HeroSelect] Confirm rejected: current GameMode does not implement HeroSelectionAuthority. GM=%s HeroId=%d Context=%d"),
            *GetNameSafe(CurrentGameMode),
            HeroId,
            static_cast<int32>(Context));
        return;
    }

    HeroSelectionAuthority->HandleHeroConfirmRequest(this, HeroId, Context);
}

void AAscenduntPlayerController::ConfirmHero(int32 HeroId)
{
    Server_RequestConfirmHero(HeroId, EAscHeroSelectionContext::InGameRespawnRoom);
}

void AAscenduntPlayerController::OpenHeroSelection(EAscHeroSelectionContext Context)
{
    ShowHeroSelectionUI(Context);
    BP_OnOpenHeroSelection(Context);
}

void AAscenduntPlayerController::Server_RequestOpenRespawnRoomHeroSelection_Implementation()
{
    AMatchGameMode* MatchGM = GetWorld() ? GetWorld()->GetAuthGameMode<AMatchGameMode>() : nullptr;
    if (!MatchGM)
    {
        return;
    }

    MatchGM->RequestOpenRespawnRoomHeroSelection(this);
}

void AAscenduntPlayerController::Server_RequestOpenDeadHeroSelection_Implementation()
{
    AMatchGameMode* MatchGM = GetWorld() ? GetWorld()->GetAuthGameMode<AMatchGameMode>() : nullptr;
    if (MatchGM)
    {
        MatchGM->RequestOpenDeathHeroSelection(this);
    }
}

void AAscenduntPlayerController::CloseHeroSelection()
{
    if (!ActiveHeroSelectionWidget || !CanCloseHeroSelectionFromInput())
    {
        return;
    }

    HideHeroSelectionUI();
    ApplyInputMode_GameOnly();
}

bool AAscenduntPlayerController::IsHeroSelectionOpen() const
{
    return ActiveHeroSelectionWidget != nullptr;
}

void AAscenduntPlayerController::Client_OpenHeroSelection_Implementation(EAscHeroSelectionContext Context)
{
    OpenHeroSelection(Context);
}

void AAscenduntPlayerController::Client_CloseHeroSelection_Implementation()
{
    HideHeroSelectionUI();
    ApplyInputMode_GameOnly();
}

void AAscenduntPlayerController::Client_NotifyHeroConfirmResult_Implementation(bool bAccepted, EAscHeroSelectionRejectReason RejectReason)
{
    if (bAccepted && ShouldCloseHeroSelectionOnAcceptedConfirm())
    {
        HideHeroSelectionUI();
        ApplyInputMode_GameOnly();
    }

    BP_OnHeroConfirmResult(bAccepted, RejectReason);

    UE_LOG(LogTemp, Warning,
        TEXT("[HeroSelect] ConfirmResult Accepted=%d RejectReason=%d CloseOnAccept=%d"),
        bAccepted ? 1 : 0,
        static_cast<int32>(RejectReason),
        bAccepted && ShouldCloseHeroSelectionOnAcceptedConfirm() ? 1 : 0);
}

void AAscenduntPlayerController::Client_EnterDeathSpectating_Implementation(AActor* DeathViewActor, float RespawnEndServerTime)
{
    if (!IsLocalController())
    {
        return;
    }

    HideHeroSelectionUI();
    bDeathSpectating = true;
    DeathViewTarget = DeathViewActor;
    CurrentDeathSpectateTarget = nullptr;

    RefreshGameplayInputBlockedState();
    ApplyInputMode_GameOnly();

    if (IsValid(DeathViewTarget))
    {
        SetDeathSpectateViewTarget(DeathViewTarget);
    }
    else
    {
        CycleDeathSpectateTarget();
    }

    GetWorldTimerManager().SetTimer(
        DeathSpectateValidationTimer,
        this,
        &ThisClass::ValidateDeathSpectateTarget,
        0.5f,
        true);

    UE_LOG(LogTemp, Warning,
        TEXT("[Respawn] EnterDeathSpectating PC=%s DeathView=%s EndServerTime=%.2f"),
        *GetNameSafe(this),
        *GetNameSafe(DeathViewActor),
        RespawnEndServerTime);
}

void AAscenduntPlayerController::Client_ExitDeathSpectating_Implementation(APawn* NewPawn)
{
    if (!IsLocalController())
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(DeathSpectateValidationTimer);
    ReleaseDeathSpectateCameraBinding();
    HideHeroSelectionUI();

    bDeathSpectating = false;
    DeathViewTarget = nullptr;
    CurrentDeathSpectateTarget = nullptr;
    RefreshGameplayInputBlockedState();

    if (IsValid(NewPawn))
    {
        SetViewTarget(NewPawn);
    }

    ApplyInputMode_GameOnly();
}

void AAscenduntPlayerController::Client_ShowInGameMessage_Implementation(const FText& Message, float DurationSeconds)
{
    if (!ActiveInGameHUDWidget && IsMatchMap())
    {
        ShowMatchUI();
    }

    if (ActiveInGameHUDWidget)
    {
        ActiveInGameHUDWidget->ShowCenterMessage(Message, DurationSeconds);
    }
}

void AAscenduntPlayerController::Client_ShowHitMarker_Implementation(bool bHeadshot)
{
    if (!ActiveInGameHUDWidget && IsMatchMap())
    {
        ShowMatchUI();
    }

    if (ActiveInGameHUDWidget)
    {
        ActiveInGameHUDWidget->ShowHitMarker(bHeadshot, false);
    }
}

void AAscenduntPlayerController::Client_ShowHitAndKillMarker_Implementation(bool bHeadshot)
{
    if (!ActiveInGameHUDWidget && IsMatchMap())
    {
        ShowMatchUI();
    }

    if (ActiveInGameHUDWidget)
    {
        // The hit/headshot layer and kill layer start in the same local call.
        ActiveInGameHUDWidget->ShowHitMarker(bHeadshot, true);
    }
}

void AAscenduntPlayerController::LeaveCurrentMatchToMainMenu()
{
    if (!IsLocalController())
    {
        return;
    }

    ReturnToMainMenuInternal(MainMenuMapPath);
}

void AAscenduntPlayerController::Client_BeginPostMatch_Implementation(float TimeDilationScale)
{
    HideHeroSelectionUI();
    StopControlledPawnMovementAndClearAbilityInput();
    SetMatchGameplayBlocked(true);

    UGameplayStatics::SetGlobalTimeDilation(
        this,
        FMath::Clamp(TimeDilationScale, 0.01f, 1.0f));
}

void AAscenduntPlayerController::Client_EndPostMatchSlowMotion_Implementation()
{
    UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
}

void AAscenduntPlayerController::Client_ReturnToMainMenu_Implementation(const FString& TargetMainMenuMapPath)
{
    ReturnToMainMenuInternal(TargetMainMenuMapPath);
}

void AAscenduntPlayerController::Client_BeginPvEMissionExitTransition_Implementation(
    bool bMissionSuccess,
    float TravelDelaySeconds)
{
    if (!IsLocalController())
    {
        return;
    }

    if (UAscGameInstance* AscGameInstance = Cast<UAscGameInstance>(GetGameInstance()))
    {
        if (bMissionSuccess)
        {
            AscGameInstance->SetPendingMainMenuPresentation(
                EAscPendingMainMenuPresentation::PvEMissionSuccess);
        }
        else
        {
            AscGameInstance->ClearPendingMainMenuPresentation();
        }
    }

    if (ActiveInGameHUDWidget)
    {
        ActiveInGameHUDWidget->BeginPvEMissionExitTransition(
            bMissionSuccess,
            FMath::Max(0.0f, TravelDelaySeconds));
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        FinishPvEMissionReturnToMainMenu();
        return;
    }

    World->GetTimerManager().ClearTimer(PvEMissionReturnTimerHandle);

    const float Delay = FMath::Max(0.0f, TravelDelaySeconds);
    if (Delay <= KINDA_SMALL_NUMBER)
    {
        FinishPvEMissionReturnToMainMenu();
        return;
    }

    World->GetTimerManager().SetTimer(
        PvEMissionReturnTimerHandle,
        this,
        &ThisClass::FinishPvEMissionReturnToMainMenu,
        Delay,
        false);
}

void AAscenduntPlayerController::FinishPvEMissionReturnToMainMenu()
{
    if (!IsLocalController())
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PvEMissionReturnTimerHandle);
    }

    ReturnToMainMenuInternal(MainMenuMapPath);
}

void AAscenduntPlayerController::SetMatchGameplayBlocked(bool bBlocked)
{
    if (bMatchGameplayBlocked == bBlocked)
    {
        return;
    }

    bMatchGameplayBlocked = bBlocked;
    RefreshGameplayInputBlockedState();
}

void AAscenduntPlayerController::ReturnToMainMenuInternal(const FString& TargetMapPath)
{
    if (!IsLocalController())
    {
        return;
    }

    UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);

    HideHeroSelectionUI();
    SetMatchGameplayBlocked(false);
    ClearCurrentWidget();

    const FString ResolvedMapPath = TargetMapPath.IsEmpty()
        ? MainMenuMapPath
        : TargetMapPath;

    ClientTravel(ResolvedMapPath, TRAVEL_Absolute);
}

void AAscenduntPlayerController::ShowHeroSelectionUI(EAscHeroSelectionContext Context)
{
    if (ActiveHeroSelectionWidget)
    {
        HideHeroSelectionUI();
    }

    if (HeroSelectionWidgetClass)
    {
        ActiveHeroSelectionWidget = CreateWidget<UAscCharacterSelectionWidget>(this, HeroSelectionWidgetClass);
        if (ActiveHeroSelectionWidget)
        {
            ActiveHeroSelectionWidget->InitializeCharacterSelection(this, Context);
            ActiveHeroSelectionWidget->AddToViewport(100);
        }
    }

    StopControlledPawnMovementAndClearAbilityInput();
    SetHeroSelectionInputBlocked(true);
    ApplyInputMode_UIOnly();

    UE_LOG(LogTemp, Warning,
        TEXT("[HeroSelect] OpenHeroSelection Context=%d WidgetClass=%s Widget=%s"),
        static_cast<int32>(Context),
        *GetNameSafe(HeroSelectionWidgetClass),
        *GetNameSafe(ActiveHeroSelectionWidget));
}

void AAscenduntPlayerController::HideHeroSelectionUI()
{
    if (ActiveHeroSelectionWidget)
    {
        ActiveHeroSelectionWidget->RemoveFromParent();
        ActiveHeroSelectionWidget = nullptr;
    }

    SetHeroSelectionInputBlocked(false);
}

void AAscenduntPlayerController::StopControlledPawnMovementAndClearAbilityInput()
{
    if (APawn* ControlledPawn = GetPawn())
    {
        if (UPawnMovementComponent* MovementComponent = ControlledPawn->GetMovementComponent())
        {
            MovementComponent->StopMovementImmediately();
        }

        if (ACharacter* Char = Cast<ACharacter>(ControlledPawn))
        {
            if (UCharacterMovementComponent* CharacterMovement = Char->GetCharacterMovement())
            {
                CharacterMovement->StopMovementImmediately();
            }
        }
    }

    if (const AAscenduntPlayerState* AscPS = GetPlayerState<AAscenduntPlayerState>())
    {
        if (UAscenduntAbilitySystemComponent* ASC = AscPS->GetAscenduntAbilitySystemComponent())
        {
            ASC->ClearAbilityInput();
        }
    }
}

void AAscenduntPlayerController::HandleSpectateNextPressed()
{
    if (bDeathSpectating && !IsHeroSelectionOpen())
    {
        CycleDeathSpectateTarget();
    }
}

void AAscenduntPlayerController::HandleDeathSpectateMouseYaw(float Value)
{
    if (!bDeathSpectating || IsHeroSelectionOpen() || FMath::IsNearlyZero(Value))
    {
        return;
    }

    AddYawInput(Value * DeathSpectateMouseYawScale);
}

void AAscenduntPlayerController::HandleDeathSpectateMousePitch(float Value)
{
    if (!bDeathSpectating || IsHeroSelectionOpen() || FMath::IsNearlyZero(Value))
    {
        return;
    }

    AddPitchInput(Value * DeathSpectateMousePitchScale);
}

void AAscenduntPlayerController::HandleDeadHeroSelectionPressed()
{
    if (!bDeathSpectating)
    {
        return;
    }

    if (IsHeroSelectionOpen())
    {
        CloseHeroSelection();
        return;
    }

    Server_RequestOpenDeadHeroSelection();
}

void AAscenduntPlayerController::GatherAliveAlliedSpectateTargets(TArray<AActor*>& OutTargets) const
{
    OutTargets.Reset();

    const AAscenduntPlayerState* LocalPS = GetPlayerState<AAscenduntPlayerState>();
    const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
    if (!LocalPS || !GS)
    {
        return;
    }

    for (APlayerState* PS : GS->PlayerArray)
    {
        AAscenduntPlayerState* OtherPS = Cast<AAscenduntPlayerState>(PS);
        APawn* OtherPawn = OtherPS ? OtherPS->GetPawn() : nullptr;
        if (!OtherPS || OtherPS == LocalPS || OtherPS->GetTeamId() != LocalPS->GetTeamId() ||
            OtherPS->GetRespawnState() != EAscPlayerRespawnState::Alive || !IsValid(OtherPawn))
        {
            continue;
        }

        OutTargets.Add(OtherPawn);
    }

    OutTargets.Sort([](const AActor& A, const AActor& B)
    {
        const APawn* PawnA = Cast<APawn>(&A);
        const APawn* PawnB = Cast<APawn>(&B);
        const APlayerState* PSA = PawnA ? PawnA->GetPlayerState() : nullptr;
        const APlayerState* PSB = PawnB ? PawnB->GetPlayerState() : nullptr;
        return (PSA ? PSA->GetPlayerId() : INDEX_NONE) < (PSB ? PSB->GetPlayerId() : INDEX_NONE);
    });
}

bool AAscenduntPlayerController::IsValidAliveAlliedSpectateTarget(const AActor* Target) const
{
    const APawn* TargetPawn = Cast<APawn>(Target);
    const AAscenduntPlayerState* TargetPS = TargetPawn ? TargetPawn->GetPlayerState<AAscenduntPlayerState>() : nullptr;
    const AAscenduntPlayerState* LocalPS = GetPlayerState<AAscenduntPlayerState>();

    return IsValid(TargetPawn) && TargetPS && LocalPS && TargetPS != LocalPS &&
        TargetPS->GetTeamId() == LocalPS->GetTeamId() &&
        TargetPS->GetRespawnState() == EAscPlayerRespawnState::Alive;
}

void AAscenduntPlayerController::CycleDeathSpectateTarget()
{
    if (!bDeathSpectating)
    {
        return;
    }

    TArray<AActor*> AlliedTargets;
    GatherAliveAlliedSpectateTargets(AlliedTargets);

    if (CurrentDeathSpectateTarget == DeathViewTarget)
    {
        SetDeathSpectateViewTarget(AlliedTargets.Num() > 0 ? AlliedTargets[0] : DeathViewTarget.Get());
        return;
    }

    const int32 CurrentIndex = AlliedTargets.IndexOfByKey(CurrentDeathSpectateTarget.Get());
    if (CurrentIndex != INDEX_NONE && AlliedTargets.IsValidIndex(CurrentIndex + 1))
    {
        SetDeathSpectateViewTarget(AlliedTargets[CurrentIndex + 1]);
        return;
    }

    if (IsValid(DeathViewTarget))
    {
        SetDeathSpectateViewTarget(DeathViewTarget);
    }
    else if (AlliedTargets.Num() > 0)
    {
        SetDeathSpectateViewTarget(AlliedTargets[0]);
    }
}

void AAscenduntPlayerController::SetDeathSpectateViewTarget(AActor* NewTarget)
{
    if (!bDeathSpectating || !IsValid(NewTarget))
    {
        return;
    }

    BindDeathSpectateCamera(NewTarget);
    CurrentDeathSpectateTarget = NewTarget;
    SetViewTargetWithBlend(NewTarget, 0.2f);
}

void AAscenduntPlayerController::BindDeathSpectateCamera(AActor* Target)
{
    ReleaseDeathSpectateCameraBinding();

    UAscenduntCameraComponent* CameraComponent =
        UAscenduntCameraComponent::FindCameraComponent(Target);

    if (!CameraComponent || !DeathSpectatorCameraModeClass)
    {
        return;
    }

    // Force the same upright third-person mode for both our ragdoll corpse and an allied Pawn.
    CameraComponent->DetermineCameraModeDelegate.BindUObject(
        this,
        &ThisClass::DetermineDeathSpectateCameraMode);

    DeathSpectateBoundCamera = CameraComponent;
}

void AAscenduntPlayerController::ReleaseDeathSpectateCameraBinding()
{
    if (UAscenduntCameraComponent* CameraComponent = DeathSpectateBoundCamera.Get())
    {
        // DeathSpectateBoundCamera is assigned only after this controller binds the delegate.
        CameraComponent->DetermineCameraModeDelegate.Unbind();
    }

    DeathSpectateBoundCamera.Reset();
}

TSubclassOf<UAscenduntCameraMode> AAscenduntPlayerController::DetermineDeathSpectateCameraMode() const
{
    return DeathSpectatorCameraModeClass;
}

void AAscenduntPlayerController::ValidateDeathSpectateTarget()
{
    if (!bDeathSpectating)
    {
        GetWorldTimerManager().ClearTimer(DeathSpectateValidationTimer);
        return;
    }

    if (CurrentDeathSpectateTarget == DeathViewTarget && IsValid(DeathViewTarget))
    {
        if (GetViewTarget() != DeathViewTarget)
        {
            SetDeathSpectateViewTarget(DeathViewTarget);
        }
        return;
    }

    if (IsValidAliveAlliedSpectateTarget(CurrentDeathSpectateTarget))
    {
        if (GetViewTarget() != CurrentDeathSpectateTarget)
        {
            SetDeathSpectateViewTarget(CurrentDeathSpectateTarget);
        }
        return;
    }

    TArray<AActor*> AlliedTargets;
    GatherAliveAlliedSpectateTargets(AlliedTargets);
    if (AlliedTargets.Num() > 0)
    {
        SetDeathSpectateViewTarget(AlliedTargets[0]);
    }
    else if (IsValid(DeathViewTarget))
    {
        SetDeathSpectateViewTarget(DeathViewTarget);
    }
}

void AAscenduntPlayerController::SetHeroSelectionInputBlocked(bool bBlocked)
{
    if (bHeroSelectionInputBlocked == bBlocked)
    {
        return;
    }

    bHeroSelectionInputBlocked = bBlocked;
    RefreshGameplayInputBlockedState();
}

void AAscenduntPlayerController::RefreshGameplayInputBlockedState()
{
    const bool bShouldBlockMoveInput = bHeroSelectionInputBlocked || bMatchGameplayBlocked || bDeathSpectating;
    // Death spectating must still accept mouse look. UI and post-match blockers continue to block it.
    const bool bShouldBlockLookInput = bHeroSelectionInputBlocked || bMatchGameplayBlocked;
    const bool bShouldBlockAbilityInput = bShouldBlockMoveInput;

    // These APIs are counter-based, so move and look state must each transition exactly once.
    if (bMoveInputCurrentlyBlocked != bShouldBlockMoveInput)
    {
        SetIgnoreMoveInput(bShouldBlockMoveInput);
        bMoveInputCurrentlyBlocked = bShouldBlockMoveInput;
    }

    if (bLookInputCurrentlyBlocked != bShouldBlockLookInput)
    {
        SetIgnoreLookInput(bShouldBlockLookInput);
        bLookInputCurrentlyBlocked = bShouldBlockLookInput;
    }

    if (const AAscenduntPlayerState* AscPS = GetPlayerState<AAscenduntPlayerState>())
    {
        if (UAscenduntAbilitySystemComponent* ASC = AscPS->GetAscenduntAbilitySystemComponent())
        {
            ASC->ClearAbilityInput();

            if (bShouldBlockAbilityInput && !bAbilityInputBlockTagApplied)
            {
                ASC->AddLooseGameplayTag(TAG_Gameplay_AbilityInputBlocked);
                bAbilityInputBlockTagApplied = true;
            }
            else if (!bShouldBlockAbilityInput && bAbilityInputBlockTagApplied)
            {
                ASC->RemoveLooseGameplayTag(TAG_Gameplay_AbilityInputBlocked);
                bAbilityInputBlockTagApplied = false;
            }
        }
    }
}

bool AAscenduntPlayerController::CanCloseHeroSelectionFromInput() const
{
    if (!ActiveHeroSelectionWidget)
    {
        return false;
    }

    // Initial timed character selection should stay authoritative. H/Esc closes
    // only the in-game respawn-room UI opened by the player.
    const EAscHeroSelectionContext Context = ActiveHeroSelectionWidget->GetSelectionContext();
    return Context == EAscHeroSelectionContext::InGameRespawnRoom ||
        Context == EAscHeroSelectionContext::DeadRespawn;
}

bool AAscenduntPlayerController::ShouldCloseHeroSelectionOnAcceptedConfirm() const
{
    if (!ActiveHeroSelectionWidget)
    {
        return true;
    }

    if (ActiveHeroSelectionWidget->GetSelectionContext() != EAscHeroSelectionContext::MatchInitialSelect)
    {
        return true;
    }

    // Initial selection is a lock-in operation. Individual players must keep the
    // selection screen open until the authoritative GameMode advances the whole
    // match out of its hero-selection phase and closes the UI for everyone.
    if (const AMatchGameState* MatchGS = GetWorld() ? GetWorld()->GetGameState<AMatchGameState>() : nullptr)
    {
        return MatchGS->GetMatchPhase() != EAscMatchPhase::CharacterSelect;
    }

    if (const AAscPvEGameState* PvEGS = GetWorld() ? GetWorld()->GetGameState<AAscPvEGameState>() : nullptr)
    {
        return PvEGS->GetGamePhase() != EAscPvEGamePhase::HeroSelection;
    }

    return true;
}

AAscenduntPlayerState* AAscenduntPlayerController::GetAscenduntPS() const
{
    return GetPlayerState<AAscenduntPlayerState>();
}

void AAscenduntPlayerController::RegisterActiveHeroInputComponent(UHeroInputComponent* NewComponent)
{
    if (!IsLocalController() || !NewComponent || ActiveHeroInputComponent == NewComponent)
    {
        return;
    }

    UHeroInputComponent* PreviousComponent = ActiveHeroInputComponent;
    ActiveHeroInputComponent = NewComponent;

    // Force the previous owner to remove its contexts before the new owner installs its own.
    if (IsValid(PreviousComponent))
    {
        PreviousComponent->UninitializePlayerInput(true);
    }
}

void AAscenduntPlayerController::UnregisterActiveHeroInputComponent(UHeroInputComponent* Component)
{
    if (ActiveHeroInputComponent == Component)
    {
        ActiveHeroInputComponent = nullptr;
    }
}

bool AAscenduntPlayerController::IsActiveHeroInputComponent(const UHeroInputComponent* Component) const
{
    return ActiveHeroInputComponent == Component;
}
