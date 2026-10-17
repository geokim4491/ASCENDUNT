#include "MatchGameMode.h"

#include "MatchGameState.h"
#include "ASCENDUNT/Player/AscenduntPlayerController.h"
#include "ASCENDUNT/Player/AscenduntPlayerState.h"
#include "ASCENDUNT/HeroSelection/AscHeroSelectionComponent.h"
#include "ASCENDUNT/Character/Hero/HeroCharacter.h"
#include "ASCENDUNT/Character/Hero/HeroPawnData.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"
#include "ASCENDUNT/Map/SpawnRoom/AscRespawnRoomVolume.h"
#include "Engine/World.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "TimerManager.h"

AMatchGameMode::AMatchGameMode()
{
    HeroSelectionComponent = CreateDefaultSubobject<UAscHeroSelectionComponent>(TEXT("HeroSelectionComponent"));

    PlayerControllerClass = AAscenduntPlayerController::StaticClass();
    PlayerStateClass = AAscenduntPlayerState::StaticClass();
    GameStateClass = AMatchGameState::StaticClass();

    // Match players should not spawn a combat pawn before hero confirmation.
    // Confirmed players are spawned when PreRoundWaiting starts or when they confirm late.
    DefaultPawnClass = nullptr;
    bStartPlayersAsSpectators = true;

    bUseSeamlessTravel = true;
    bDelayedStart = true;
}

void AMatchGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);

    BuildMatchSettingsFromOptions(MapName, Options);

    UE_LOG(LogTemp, Warning,
        TEXT("[MatchGM] InitGame MatchId=%s Map=%s Mode=%d Required=%d Max=%d TeamSize=%d ScoreLimit=%d TimeLimit=%d Intro=%d CharSelect=%d PreRound=%d"),
        *PendingMatchSettings.MatchId,
        *PendingMatchSettings.SelectedMapPath,
        static_cast<int32>(PendingMatchSettings.Rules.Mode),
        PendingMatchSettings.Rules.RequiredPlayers,
        PendingMatchSettings.Rules.MaxPlayers,
        PendingMatchSettings.Rules.TeamSize,
        PendingMatchSettings.Rules.ScoreLimit,
        PendingMatchSettings.Rules.TimeLimitSeconds,
        PendingMatchSettings.Rules.MapIntroSeconds,
        PendingMatchSettings.Rules.CharacterSelectSeconds,
        PendingMatchSettings.Rules.PreRoundWaitingSeconds);
}

void AMatchGameMode::InitGameState()
{
    Super::InitGameState();

    if (AMatchGameState* MatchGS = GetAscMatchGameState())
    {
        MatchGS->SetMatchSettings(PendingMatchSettings);
        MatchGS->SetMatchPhase(EAscMatchPhase::WaitingForPlayers);

        FAscMatchClockState InitialClock;
        InitialClock.ElapsedTimeSeconds = 0;
        InitialClock.RemainingTimeSeconds = PendingMatchSettings.Rules.TimeLimitSeconds;
        InitialClock.PhaseRemainingSeconds = 0;
        MatchGS->SetClockState(InitialClock);
        MatchGS->SetMatchResult(FAscMatchResultState());

        InitializeDefaultTeamStates();
        UpdateConnectedPlayerCount();
    }
}

void AMatchGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Python orchestrator waits for this token before returning tickets as ready.
    UE_LOG(LogTemp, Warning,
        TEXT("[DS_READY] MatchId=%s Port=%d Map=%s"),
        *PendingMatchSettings.MatchId,
        GetWorld() ? GetWorld()->URL.Port : 0,
        *PendingMatchSettings.SelectedMapPath);
}

void AMatchGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CleanupAllRespawnRuntime();
    Super::EndPlay(EndPlayReason);
}


AActor* AMatchGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    const AAscenduntPlayerState* AscPS = Player ? Player->GetPlayerState<AAscenduntPlayerState>() : nullptr;
    const FName RequiredStartTag = AscPS ? GetPlayerStartTagForTeam(AscPS->GetTeamId()) : NAME_None;

    TArray<APlayerStart*> AvailableTeamStarts;
    TArray<APlayerStart*> TeamStarts;

    if (!RequiredStartTag.IsNone())
    {
        for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
        {
            APlayerStart* PlayerStart = *It;
            if (!DoesPlayerStartMatchTeamTag(PlayerStart, RequiredStartTag))
            {
                continue;
            }

            TeamStarts.Add(PlayerStart);
            if (IsPlayerStartAvailable(PlayerStart, Player))
            {
                AvailableTeamStarts.Add(PlayerStart);
            }
        }
    }

    if (AvailableTeamStarts.Num() > 0)
    {
        APlayerStart* SelectedStart = AvailableTeamStarts[FMath::RandRange(0, AvailableTeamStarts.Num() - 1)];
        UE_LOG(LogTemp, Warning,
            TEXT("[MatchGM] ChoosePlayerStart Team=%d Tag=%s Start=%s"),
            AscPS ? AscPS->GetTeamId() : 0,
            *RequiredStartTag.ToString(),
            *GetNameSafe(SelectedStart));
        return SelectedStart;
    }

    if (TeamStarts.Num() > 0)
    {
        APlayerStart* SelectedStart = TeamStarts[FMath::RandRange(0, TeamStarts.Num() - 1)];
        UE_LOG(LogTemp, Warning,
            TEXT("[MatchGM] ChoosePlayerStart using occupied team start fallback. Team=%d Tag=%s Start=%s"),
            AscPS ? AscPS->GetTeamId() : 0,
            *RequiredStartTag.ToString(),
            *GetNameSafe(SelectedStart));
        return SelectedStart;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[MatchGM] ChoosePlayerStart fallback to Super. Team=%d RequiredTag=%s"),
        AscPS ? AscPS->GetTeamId() : 0,
        *RequiredStartTag.ToString());

    return Super::ChoosePlayerStart_Implementation(Player);
}

UClass* AMatchGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
    const AAscenduntPlayerState* AscPS = InController ? InController->GetPlayerState<AAscenduntPlayerState>() : nullptr;
    const UHeroPawnData* PawnData = AscPS ? AscPS->GetPawnData() : nullptr;

    if (PawnData && PawnData->PawnClass)
    {
        return PawnData->PawnClass;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[MatchGM] GetDefaultPawnClass fallback. Controller=%s PawnData=%s"),
        *GetNameSafe(InController),
        *GetNameSafe(PawnData));

    return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void AMatchGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);

    if (!ErrorMessage.IsEmpty())
    {
        return;
    }

    if (!CanAcceptPlayer())
    {
        ErrorMessage = TEXT("MATCH_FULL");
        UE_LOG(LogTemp, Warning,
            TEXT("[MatchGM] PreLogin rejected: MATCH_FULL Current=%d Max=%d Address=%s"),
            GameState ? GameState->PlayerArray.Num() : GetNumPlayers(),
            PendingMatchSettings.Rules.MaxPlayers,
            *Address);
        return;
    }

    if (const AMatchGameState* MatchGS = GetAscMatchGameState())
    {
        const EAscMatchPhase Phase = MatchGS->GetMatchPhase();
        if (Phase == EAscMatchPhase::Playing ||
            Phase == EAscMatchPhase::PostMatch ||
            Phase == EAscMatchPhase::Returning)
        {
            ErrorMessage = TEXT("MATCH_ALREADY_STARTED");
            UE_LOG(LogTemp, Warning,
                TEXT("[MatchGM] PreLogin rejected: MATCH_ALREADY_STARTED Address=%s"),
                *Address);
            return;
        }
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[MatchGM] PreLogin accepted Address=%s Options=%s"),
        *Address,
        *Options);
}

FString AMatchGameMode::InitNewPlayer(
    APlayerController* NewPlayerController,
    const FUniqueNetIdRepl& UniqueId,
    const FString& Options,
    const FString& Portal)
{
    const FString Error = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);

    if (NewPlayerController)
    {
        PendingPlayerOptions.Add(NewPlayerController, Options);
    }

    return Error;
}

void AMatchGameMode::PostLogin(APlayerController* NewPlayer)
{
    FString Options;
    if (const FString* FoundOptions = PendingPlayerOptions.Find(NewPlayer))
    {
        Options = *FoundOptions;
    }

    // TeamId must be assigned before Super::PostLogin() can select/cache a StartSpot.
    // Otherwise late RestartPlayer() calls can reuse a Team1 StartSpot for a Team2 player.
    RegisterPlayerInMatchState(NewPlayer, Options);

    Super::PostLogin(NewPlayer);

    UpdateConnectedPlayerCount();

    if (const AMatchGameState* MatchGS = GetAscMatchGameState())
    {
        if (MatchGS->GetConnectedPlayerCount() >= PendingMatchSettings.Rules.RequiredPlayers &&
            MatchGS->GetMatchPhase() == EAscMatchPhase::WaitingForPlayers)
        {
            StartMapIntroPhase();
        }
    }
}

void AMatchGameMode::Logout(AController* Exiting)
{
    int32 ExitingPlayerStateId = INDEX_NONE;

    if (APlayerController* PC = Cast<APlayerController>(Exiting))
    {
        if (const APlayerState* PS = PC->PlayerState)
        {
            ExitingPlayerStateId = PS->GetPlayerId();
        }

        PendingPlayerOptions.Remove(PC);

        if (AAscenduntPlayerController* AscPC = Cast<AAscenduntPlayerController>(PC))
        {
            CleanupRespawnRuntime(AscPC);
        }
    }

    Super::Logout(Exiting);

    if (AMatchGameState* MatchGS = GetAscMatchGameState())
    {
        if (ExitingPlayerStateId != INDEX_NONE)
        {
            MatchGS->MarkPlayerDisconnected(ExitingPlayerStateId);
        }

        UpdateConnectedPlayerCount();
    }
}

void AMatchGameMode::StartMapIntroPhase()
{
    StopPhaseCountdown();
    GetWorldTimerManager().ClearTimer(PhaseTransitionTimerHandle);

    if (AMatchGameState* MatchGS = GetAscMatchGameState())
    {
        MatchGS->SetMatchPhase(EAscMatchPhase::MapIntro);
        UE_LOG(LogTemp, Warning, TEXT("[MatchGM] Phase -> MapIntro Duration=%d"), PendingMatchSettings.Rules.MapIntroSeconds);
    }

    BeginPhaseCountdown(PendingMatchSettings.Rules.MapIntroSeconds);

    if (PendingMatchSettings.Rules.MapIntroSeconds <= 0)
    {
        StartCharacterSelectPhase();
        return;
    }

    GetWorldTimerManager().SetTimer(
        PhaseTransitionTimerHandle,
        this,
        &ThisClass::StartCharacterSelectPhase,
        static_cast<float>(PendingMatchSettings.Rules.MapIntroSeconds),
        false);
}

void AMatchGameMode::StartCharacterSelectPhase()
{
    StopPhaseCountdown();
    GetWorldTimerManager().ClearTimer(PhaseTransitionTimerHandle);

    if (AMatchGameState* MatchGS = GetAscMatchGameState())
    {
        MatchGS->SetMatchPhase(EAscMatchPhase::CharacterSelect);
        UE_LOG(LogTemp, Warning, TEXT("[MatchGM] Phase -> CharacterSelect Duration=%d"), PendingMatchSettings.Rules.CharacterSelectSeconds);
    }

    OpenInitialHeroSelectionForAllPlayers();
    BeginPhaseCountdown(PendingMatchSettings.Rules.CharacterSelectSeconds);

    if (PendingMatchSettings.Rules.CharacterSelectSeconds <= 0)
    {
        StartPreRoundWaitingPhase();
        return;
    }

    GetWorldTimerManager().SetTimer(
        PhaseTransitionTimerHandle,
        this,
        &ThisClass::StartPreRoundWaitingPhase,
        static_cast<float>(PendingMatchSettings.Rules.CharacterSelectSeconds),
        false);
}

void AMatchGameMode::StartPreRoundWaitingPhase()
{
    StopPhaseCountdown();
    GetWorldTimerManager().ClearTimer(PhaseTransitionTimerHandle);

    if (AMatchGameState* MatchGS = GetAscMatchGameState())
    {
        MatchGS->SetMatchPhase(EAscMatchPhase::PreRoundWaiting);
        UE_LOG(LogTemp, Warning, TEXT("[MatchGM] Phase -> PreRoundWaiting Duration=%d"), PendingMatchSettings.Rules.PreRoundWaitingSeconds);
    }

    // Confirmed players enter the closed spawn room. Unconfirmed players keep
    // the character selection UI and may still confirm during PreRoundWaiting.
    SpawnConfirmedHeroPawns();
    OpenInitialHeroSelectionForUnconfirmedPlayers();
    BeginPhaseCountdown(PendingMatchSettings.Rules.PreRoundWaitingSeconds);

    if (PendingMatchSettings.Rules.PreRoundWaitingSeconds <= 0)
    {
        StartPlayingPhase();
        return;
    }

    GetWorldTimerManager().SetTimer(
        PhaseTransitionTimerHandle,
        this,
        &ThisClass::StartPlayingPhase,
        static_cast<float>(PendingMatchSettings.Rules.PreRoundWaitingSeconds),
        false);
}

void AMatchGameMode::StartPlayingPhase()
{
    StopPhaseCountdown();
    GetWorldTimerManager().ClearTimer(PhaseTransitionTimerHandle);
    GetWorldTimerManager().ClearTimer(PostMatchStageTimerHandle);

    bMatchFinalized = false;
    CurrentMatchElapsedSeconds = 0;

    if (AMatchGameState* MatchGS = GetAscMatchGameState())
    {
        MatchGS->SetMatchResult(FAscMatchResultState());
        MatchGS->SetMatchPhase(EAscMatchPhase::Playing);

        FAscMatchClockState PlayingClock = MatchGS->GetClockState();
        PlayingClock.ElapsedTimeSeconds = 0;
        PlayingClock.PhaseRemainingSeconds = 0;
        PlayingClock.RemainingTimeSeconds = PendingMatchSettings.Rules.TimeLimitSeconds;
        MatchGS->SetClockState(PlayingClock);

        StartMatch();
        StartMatchClock();
        UE_LOG(LogTemp, Warning, TEXT("[MatchGM] Phase -> Playing"));
    }
}

bool AMatchGameMode::AddTeamScore(uint8 TeamId, int32 ScoreDelta)
{
    AMatchGameState* MatchGS = GetAscMatchGameState();
    if (!HasAuthority() || !MatchGS || bMatchFinalized ||
        MatchGS->GetMatchPhase() != EAscMatchPhase::Playing ||
        TeamId == 0 || ScoreDelta == 0)
    {
        return false;
    }

    const bool bKnownTeam = MatchGS->GetTeamStates().ContainsByPredicate(
        [TeamId](const FAscTeamMatchState& TeamState)
        {
            return TeamState.TeamId == TeamId;
        });

    if (!bKnownTeam)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[MatchGM] AddTeamScore rejected: unknown TeamId=%d"),
            TeamId);
        return false;
    }

    const int32 PreviousScore = MatchGS->GetTeamScore(TeamId);
    const int32 NewScore = FMath::Max(0, PreviousScore + ScoreDelta);
    MatchGS->SetTeamScore(TeamId, NewScore);

    UE_LOG(LogTemp, Warning,
        TEXT("[MatchGM] TeamScore Team=%d Previous=%d Delta=%d New=%d Limit=%d"),
        TeamId,
        PreviousScore,
        ScoreDelta,
        NewScore,
        PendingMatchSettings.Rules.ScoreLimit);

    if (PendingMatchSettings.Rules.ScoreLimit > 0 &&
        NewScore >= PendingMatchSettings.Rules.ScoreLimit)
    {
        TryFinalizeMatch(EAscMatchEndReason::ScoreLimitReached);
    }

    return true;
}

void AMatchGameMode::FinishMatch()
{
    TryFinalizeMatch(EAscMatchEndReason::Error);
}

void AMatchGameMode::StartMatchClock()
{
    StopMatchClock();

    GetWorldTimerManager().SetTimer(
        MatchClockTimerHandle,
        this,
        &ThisClass::TickMatchClock,
        1.0f,
        true);
}

void AMatchGameMode::StopMatchClock()
{
    GetWorldTimerManager().ClearTimer(MatchClockTimerHandle);
}

void AMatchGameMode::TickMatchClock()
{
    AMatchGameState* MatchGS = GetAscMatchGameState();
    if (!MatchGS || bMatchFinalized || MatchGS->GetMatchPhase() != EAscMatchPhase::Playing)
    {
        StopMatchClock();
        return;
    }

    ++CurrentMatchElapsedSeconds;

    FAscMatchClockState ClockState = MatchGS->GetClockState();
    ClockState.ElapsedTimeSeconds = CurrentMatchElapsedSeconds;
    ClockState.PhaseRemainingSeconds = 0;

    const int32 TimeLimitSeconds = PendingMatchSettings.Rules.TimeLimitSeconds;
    ClockState.RemainingTimeSeconds = TimeLimitSeconds > 0
        ? FMath::Max(0, TimeLimitSeconds - CurrentMatchElapsedSeconds)
        : 0;

    MatchGS->SetClockState(ClockState);

    if (TimeLimitSeconds > 0 && ClockState.RemainingTimeSeconds <= 0)
    {
        TryFinalizeMatch(EAscMatchEndReason::TimeLimitExpired);
    }
}

bool AMatchGameMode::TryFinalizeMatch(EAscMatchEndReason EndReason)
{
    AMatchGameState* MatchGS = GetAscMatchGameState();
    if (!HasAuthority() || !MatchGS || bMatchFinalized)
    {
        return false;
    }

    bMatchFinalized = true;

    StopPhaseCountdown();
    StopMatchClock();
    GetWorldTimerManager().ClearTimer(PhaseTransitionTimerHandle);
    GetWorldTimerManager().ClearTimer(PostMatchStageTimerHandle);
    CleanupAllRespawnRuntime();

    FAscMatchResultState Result;
    Result.bFinalized = true;
    Result.EndReason = EndReason;
    Result.WinningTeamId = EndReason == EAscMatchEndReason::Error
        ? 0
        : ResolveWinningTeamId();

    MatchGS->SetMatchResult(Result);
    MatchGS->SetMatchPhase(EAscMatchPhase::PostMatch);

    SetGameplayBlockedForAllPlayers(true);
    EndMatch();

    UE_LOG(LogTemp, Warning,
        TEXT("[MatchGM] Match finalized Reason=%d WinningTeam=%d"),
        static_cast<int32>(EndReason),
        Result.WinningTeamId);

    StartResultSlowMotionStage();
    return true;
}

uint8 AMatchGameMode::ResolveWinningTeamId() const
{
    const AMatchGameState* MatchGS = GetAscMatchGameState();
    if (!MatchGS)
    {
        return 0;
    }

    uint8 WinningTeamId = 0;
    int32 HighestScore = MIN_int32;
    bool bTieForHighest = false;

    for (const FAscTeamMatchState& TeamState : MatchGS->GetTeamStates())
    {
        if (TeamState.TeamId == 0)
        {
            continue;
        }

        if (TeamState.Score > HighestScore)
        {
            HighestScore = TeamState.Score;
            WinningTeamId = TeamState.TeamId;
            bTieForHighest = false;
        }
        else if (TeamState.Score == HighestScore)
        {
            bTieForHighest = true;
        }
    }

    return bTieForHighest ? 0 : WinningTeamId;
}

void AMatchGameMode::StartResultSlowMotionStage()
{
    SetPostMatchStage(EAscPostMatchStage::ResultSlowMotion, ResultSlowMotionDurationSeconds);

    if (GetWorld())
    {
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            if (AAscenduntPlayerController* AscPC = Cast<AAscenduntPlayerController>(It->Get()))
            {
                AscPC->Client_BeginPostMatch(ResultSlowMotionScale);
            }
        }
    }

    if (ResultSlowMotionDurationSeconds <= 0.0f)
    {
        StartWinnerShowcaseStage();
        return;
    }

    GetWorldTimerManager().SetTimer(
        PostMatchStageTimerHandle,
        this,
        &ThisClass::StartWinnerShowcaseStage,
        ResultSlowMotionDurationSeconds,
        false);
}

void AMatchGameMode::StartWinnerShowcaseStage()
{
    GetWorldTimerManager().ClearTimer(PostMatchStageTimerHandle);
    SetPostMatchStage(EAscPostMatchStage::WinnerShowcase, WinnerShowcaseDurationSeconds);

    if (GetWorld())
    {
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            if (AAscenduntPlayerController* AscPC = Cast<AAscenduntPlayerController>(It->Get()))
            {
                AscPC->Client_EndPostMatchSlowMotion();
            }
        }
    }

    // Winner presentation actors/camera are intentionally left for the next patch.
    if (WinnerShowcaseDurationSeconds <= 0.0f)
    {
        StartHighlightStage();
        return;
    }

    GetWorldTimerManager().SetTimer(
        PostMatchStageTimerHandle,
        this,
        &ThisClass::StartHighlightStage,
        WinnerShowcaseDurationSeconds,
        false);
}

void AMatchGameMode::StartHighlightStage()
{
    GetWorldTimerManager().ClearTimer(PostMatchStageTimerHandle);
    SetPostMatchStage(EAscPostMatchStage::Highlight, HighlightDurationSeconds);

    if (HighlightDurationSeconds <= 0.0f)
    {
        StartReturningStage();
        return;
    }

    GetWorldTimerManager().SetTimer(
        PostMatchStageTimerHandle,
        this,
        &ThisClass::StartReturningStage,
        HighlightDurationSeconds,
        false);
}

void AMatchGameMode::StartReturningStage()
{
    GetWorldTimerManager().ClearTimer(PostMatchStageTimerHandle);

    if (AMatchGameState* MatchGS = GetAscMatchGameState())
    {
        MatchGS->SetMatchPhase(EAscMatchPhase::Returning);
    }

    SetPostMatchStage(EAscPostMatchStage::Returning, 0.0f);

    if (!GetWorld())
    {
        return;
    }

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (AAscenduntPlayerController* AscPC = Cast<AAscenduntPlayerController>(It->Get()))
        {
            AscPC->Client_ReturnToMainMenu(ReturnMainMenuMapPath);
        }
    }
}

void AMatchGameMode::SetPostMatchStage(EAscPostMatchStage NewStage, float DurationSeconds)
{
    if (AMatchGameState* MatchGS = GetAscMatchGameState())
    {
        const float StageEndServerTime = DurationSeconds > 0.0f
            ? MatchGS->GetServerWorldTimeSeconds() + DurationSeconds
            : 0.0f;

        MatchGS->SetPostMatchStage(NewStage, StageEndServerTime);
    }
}

void AMatchGameMode::SetGameplayBlockedForAllPlayers(bool bBlocked)
{
    if (!GetWorld())
    {
        return;
    }

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (AAscenduntPlayerController* AscPC = Cast<AAscenduntPlayerController>(It->Get()))
        {
            AscPC->SetMatchGameplayBlocked(bBlocked);
        }
    }
}

void AMatchGameMode::BuildMatchSettingsFromOptions(const FString& MapName, const FString& Options)
{
    PendingMatchSettings.SelectedMapPath = MapName;
    PendingMatchSettings.SelectedMapDisplayName = ParseDecodedOption(Options, TEXT("AscMapName"), MapName);
    PendingMatchSettings.MatchId = UGameplayStatics::ParseOption(Options, TEXT("AscMatchId"));
    PendingMatchSettings.MatchName = ParseDecodedOption(Options, TEXT("AscMatchName"), TEXT("Quick Match"));

    FAscMatchRuleSettings& Rules = PendingMatchSettings.Rules;

    // 1) BP_GM에서 고른 Mode를 먼저 적용한다. URL AscMatchMode가 있으면 서버 실행 옵션이 최종 우선권을 가진다.
    const FString ModeString = UGameplayStatics::ParseOption(Options, TEXT("AscMatchMode"));
    Rules.Mode = ModeString.IsEmpty()
        ? GameModeMatchRules.Mode
        : ParseMatchMode(ModeString, GameModeMatchRules.Mode);

    const FString MaxPlayersString = UGameplayStatics::ParseOption(Options, TEXT("AscMaxPlayers"));
    const FString RequiredPlayersString = UGameplayStatics::ParseOption(Options, TEXT("AscRequiredPlayers"));
    const FString TeamSizeString = UGameplayStatics::ParseOption(Options, TEXT("AscTeamSize"));
    const FString ScoreLimitString = UGameplayStatics::ParseOption(Options, TEXT("AscScoreLimit"));
    const FString TimeLimitString = UGameplayStatics::ParseOption(Options, TEXT("AscTimeLimitSeconds"));
    const FString MapIntroString = UGameplayStatics::ParseOption(Options, TEXT("AscMapIntroSeconds"));
    const FString CharacterSelectString = UGameplayStatics::ParseOption(Options, TEXT("AscCharacterSelectSeconds"));
    const FString PreRoundWaitingString = UGameplayStatics::ParseOption(Options, TEXT("AscPreRoundWaitingSeconds"));
    const FString TeamBasedString = UGameplayStatics::ParseOption(Options, TEXT("AscTeamBased"));
    const FString AllowDuplicateHeroesString = UGameplayStatics::ParseOption(Options, TEXT("AscAllowDuplicateHeroes"));
    const FString AllowDuplicateRolesString = UGameplayStatics::ParseOption(Options, TEXT("AscAllowDuplicateHeroRoles"));
    const FString CheckDuplicateRulesWithinTeamOnlyString = UGameplayStatics::ParseOption(Options, TEXT("AscCheckDuplicateRulesWithinTeamOnly"));

    // 2) Mode 프리셋은 인원/팀 구조를 확정한다.
    // BP_GM에는 RequiredPlayers / MaxPlayers / TeamSize를 노출하지 않는다.
    switch (Rules.Mode)
    {
    case EAscMatchMode::Duel1v1:
        Rules.RequiredPlayers = 2;
        Rules.MaxPlayers = 2;
        Rules.TeamSize = 1;
        Rules.ScoreLimit = 5;
        Rules.bTeamBased = true;
        break;

    case EAscMatchMode::TDM2v2:
        Rules.RequiredPlayers = 4;
        Rules.MaxPlayers = 4;
        Rules.TeamSize = 2;
        Rules.ScoreLimit = 10;
        Rules.bTeamBased = true;
        break;

    case EAscMatchMode::Main3v3:
        Rules.RequiredPlayers = 6;
        Rules.MaxPlayers = 6;
        Rules.TeamSize = 3;
        Rules.ScoreLimit = 3;
        Rules.bTeamBased = true;
        break;

    case EAscMatchMode::Practice:
        Rules.RequiredPlayers = 1;
        Rules.MaxPlayers = 6;
        Rules.TeamSize = 1;
        Rules.ScoreLimit = 0;
        Rules.bTeamBased = false;
        break;

    case EAscMatchMode::FreeForAll:
    default:
        Rules.RequiredPlayers = 2;
        Rules.MaxPlayers = 6;
        Rules.TeamSize = 1;
        Rules.ScoreLimit = 30;
        Rules.bTeamBased = false;
        break;
    }

    // 3) BP_GM에서 바꾸고 싶은 튜닝값만 프리셋 위에 적용한다.
    Rules.ScoreLimit = FMath::Max(0, GameModeMatchRules.ScoreLimit);
    Rules.TimeLimitSeconds = FMath::Max(0, GameModeMatchRules.TimeLimitSeconds);
    Rules.MapIntroSeconds = FMath::Max(0, GameModeMatchRules.MapIntroSeconds);
    Rules.CharacterSelectSeconds = FMath::Max(0, GameModeMatchRules.CharacterSelectSeconds);
    Rules.PreRoundWaitingSeconds = FMath::Max(0, GameModeMatchRules.PreRoundWaitingSeconds);
    Rules.HeroSelectionRules = GameModeMatchRules.HeroSelectionRules;

    // 4) Dedicated Server / Python orchestrator URL 옵션은 마지막에 적용한다.
    // 인원 관련 URL 옵션은 기존처럼 테스트/오케스트레이터에서 강제할 수 있게 유지한다.
    if (!AllowDuplicateHeroesString.IsEmpty())
    {
        Rules.HeroSelectionRules.bAllowDuplicateHeroes = ParseBoolOption(AllowDuplicateHeroesString, Rules.HeroSelectionRules.bAllowDuplicateHeroes);
    }

    if (!AllowDuplicateRolesString.IsEmpty())
    {
        Rules.HeroSelectionRules.bAllowDuplicateHeroRoles = ParseBoolOption(AllowDuplicateRolesString, Rules.HeroSelectionRules.bAllowDuplicateHeroRoles);
    }

    if (!CheckDuplicateRulesWithinTeamOnlyString.IsEmpty())
    {
        Rules.HeroSelectionRules.bCheckDuplicateRulesWithinTeamOnly = ParseBoolOption(CheckDuplicateRulesWithinTeamOnlyString, Rules.HeroSelectionRules.bCheckDuplicateRulesWithinTeamOnly);
    }

    if (!MaxPlayersString.IsEmpty())
    {
        Rules.MaxPlayers = FMath::Max(1, FCString::Atoi(*MaxPlayersString));
    }

    if (!RequiredPlayersString.IsEmpty())
    {
        Rules.RequiredPlayers = FMath::Clamp(FCString::Atoi(*RequiredPlayersString), 1, Rules.MaxPlayers);
    }
    else
    {
        Rules.RequiredPlayers = FMath::Clamp(Rules.RequiredPlayers, 1, Rules.MaxPlayers);
    }

    if (!TeamSizeString.IsEmpty())
    {
        Rules.TeamSize = FMath::Max(1, FCString::Atoi(*TeamSizeString));
    }

    if (!ScoreLimitString.IsEmpty())
    {
        Rules.ScoreLimit = FMath::Max(0, FCString::Atoi(*ScoreLimitString));
    }

    if (!TimeLimitString.IsEmpty())
    {
        Rules.TimeLimitSeconds = FMath::Max(0, FCString::Atoi(*TimeLimitString));
    }

    if (!MapIntroString.IsEmpty())
    {
        Rules.MapIntroSeconds = FMath::Max(0, FCString::Atoi(*MapIntroString));
    }

    if (!CharacterSelectString.IsEmpty())
    {
        Rules.CharacterSelectSeconds = FMath::Max(0, FCString::Atoi(*CharacterSelectString));
    }

    if (!PreRoundWaitingString.IsEmpty())
    {
        Rules.PreRoundWaitingSeconds = FMath::Max(0, FCString::Atoi(*PreRoundWaitingString));
    }

    if (!TeamBasedString.IsEmpty())
    {
        Rules.bTeamBased = ParseBoolOption(TeamBasedString, Rules.bTeamBased);
    }
}

void AMatchGameMode::InitializeDefaultTeamStates()
{
    AMatchGameState* MatchGS = GetAscMatchGameState();
    if (!MatchGS)
    {
        return;
    }

    if (PendingMatchSettings.Rules.bTeamBased)
    {
        FAscTeamMatchState Team1;
        Team1.TeamId = 1;
        Team1.Score = 0;
        MatchGS->SetTeamState(Team1);

        FAscTeamMatchState Team2;
        Team2.TeamId = 2;
        Team2.Score = 0;
        MatchGS->SetTeamState(Team2);
    }
}

void AMatchGameMode::UpdateConnectedPlayerCount()
{
    AMatchGameState* MatchGS = GetAscMatchGameState();
    if (!MatchGS)
    {
        return;
    }

    MatchGS->SetConnectedPlayerCount(GameState ? GameState->PlayerArray.Num() : GetNumPlayers());

    if (PendingMatchSettings.Rules.bTeamBased)
    {
        FAscTeamMatchState Team1;
        Team1.TeamId = 1;
        Team1.Score = MatchGS->GetTeamScore(1);

        FAscTeamMatchState Team2;
        Team2.TeamId = 2;
        Team2.Score = MatchGS->GetTeamScore(2);

        if (GameState)
        {
            for (APlayerState* PS : GameState->PlayerArray)
            {
                const AAscenduntPlayerState* AscPS = Cast<AAscenduntPlayerState>(PS);
                if (!AscPS)
                {
                    continue;
                }

                if (AscPS->GetTeamId() == 1)
                {
                    ++Team1.ConnectedPlayers;
                    ++Team1.AlivePlayers;
                }
                else if (AscPS->GetTeamId() == 2)
                {
                    ++Team2.ConnectedPlayers;
                    ++Team2.AlivePlayers;
                }
            }
        }

        MatchGS->SetTeamState(Team1);
        MatchGS->SetTeamState(Team2);
    }
}

void AMatchGameMode::RegisterPlayerInMatchState(APlayerController* NewPlayer, const FString& Options)
{
    AAscenduntPlayerState* AscPS = NewPlayer ? NewPlayer->GetPlayerState<AAscenduntPlayerState>() : nullptr;
    AMatchGameState* MatchGS = GetAscMatchGameState();
    if (!AscPS || !MatchGS)
    {
        return;
    }

    // Stable join-order assignment. Do not use PlayerArray.Num() or replicated
    // match-state count here: both can be misleading around PostLogin/PIE.
    const int32 JoinIndex = NextJoinIndexForTeamAssignment++;

    const FString PlayerId = UGameplayStatics::ParseOption(Options, TEXT("PlayerId"));
    const FString TicketId = UGameplayStatics::ParseOption(Options, TEXT("TicketId"));
    const FString DisplayName = ParseDecodedOption(Options, TEXT("DisplayName"), AscPS->GetPlayerName());
    const FString TeamIdString = UGameplayStatics::ParseOption(Options, TEXT("TeamId"));
    const FString TeamString = TeamIdString.IsEmpty() ? UGameplayStatics::ParseOption(Options, TEXT("Team")) : TeamIdString;

    uint8 AssignedTeamId = ResolveTeamIdForJoinIndex(JoinIndex);
    if (!TeamString.IsEmpty() && TeamString.IsNumeric())
    {
        AssignedTeamId = static_cast<uint8>(FMath::Clamp(FCString::Atoi(*TeamString), 1, 255));
    }

    AscPS->SetTeamId(AssignedTeamId);

    AscPS->SetMatchPlayerId(PlayerId.IsEmpty() ? FString::FromInt(AscPS->GetPlayerId()) : PlayerId);
    AscPS->SetMatchTicketId(TicketId);
    AscPS->SetDisplayName(DisplayName);

    FAscPlayerMatchStats Stats;
    Stats.PlayerStateId = AscPS->GetPlayerId();
    Stats.PlayerId = AscPS->GetMatchPlayerId();
    Stats.TicketId = AscPS->GetMatchTicketId();
    Stats.DisplayName = AscPS->GetDisplayName();
    Stats.TeamId = AscPS->GetTeamId();
    Stats.SelectedHeroId = AscPS->GetSelectedHeroId();
    Stats.ConfirmedHeroId = AscPS->GetConfirmedHeroId();
    Stats.ConfirmedHeroRole = AscPS->GetConfirmedHeroRole();
    Stats.bHeroConfirmed = AscPS->HasConfirmedHero();
    Stats.bConnected = true;

    MatchGS->UpsertPlayerStats(Stats);

    UE_LOG(LogTemp, Warning,
        TEXT("[MatchGM] Registered player PSId=%d PlayerId=%s TicketId=%s Team=%d"),
        Stats.PlayerStateId,
        *Stats.PlayerId,
        *Stats.TicketId,
        Stats.TeamId);
}

bool AMatchGameMode::HandleHeroConfirmRequest(AAscenduntPlayerController* PlayerController, int32 HeroId, EAscHeroSelectionContext Context)
{
    return HandlePlayerHeroConfirmed(PlayerController, HeroId, Context);
}

bool AMatchGameMode::HandlePlayerHeroConfirmed(AAscenduntPlayerController* PlayerController, int32 HeroId, EAscHeroSelectionContext Context)
{
    AAscenduntPlayerState* AscPS = PlayerController ? PlayerController->GetPlayerState<AAscenduntPlayerState>() : nullptr;

    // Must be captured before ConfirmHero(), because ConfirmHero() calls SetPawnData().
    const bool bHadConfirmedHeroBeforeConfirm = AscPS && (AscPS->HasConfirmedHero() || AscPS->GetPawnData() != nullptr);
    const bool bHadPawnBeforeConfirm = PlayerController && IsValid(PlayerController->GetPawn());

    const EAscHeroSelectionContext EffectiveContext = ResolveEffectiveHeroSelectionContext(PlayerController, Context);

    EAscHeroSelectionRejectReason RejectReason = EAscHeroSelectionRejectReason::None;
    if (!CanPlayerConfirmHero(PlayerController, HeroId, EffectiveContext, RejectReason))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[HeroSelect] Confirm rejected. PC=%s HeroId=%d RequestedContext=%d EffectiveContext=%d Reason=%d"),
            *GetNameSafe(PlayerController),
            HeroId,
            static_cast<int32>(Context),
            static_cast<int32>(EffectiveContext),
            static_cast<int32>(RejectReason));

        if (PlayerController)
        {
            PlayerController->Client_NotifyHeroConfirmResult(false, RejectReason);
        }
        return false;
    }

    if (!HeroSelectionComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("[HeroSelect] Confirm rejected: HeroSelectionComponent is missing."));
        if (PlayerController)
        {
            PlayerController->Client_NotifyHeroConfirmResult(false, EAscHeroSelectionRejectReason::InvalidHero);
        }
        return false;
    }

    if (!HeroSelectionComponent->ConfirmHero(
        PlayerController,
        HeroId,
        EffectiveContext,
        PendingMatchSettings.Rules.HeroSelectionRules,
        RejectReason))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[HeroSelect] Confirm rejected by component. PC=%s HeroId=%d RequestedContext=%d EffectiveContext=%d Reason=%d"),
            *GetNameSafe(PlayerController),
            HeroId,
            static_cast<int32>(Context),
            static_cast<int32>(EffectiveContext),
            static_cast<int32>(RejectReason));

        if (PlayerController)
        {
            PlayerController->Client_NotifyHeroConfirmResult(false, RejectReason);
        }
        return false;
    }

    UpdateMatchStateHeroSelection(AscPS);

    const AMatchGameState* MatchGS = GetAscMatchGameState();
    const EAscMatchPhase Phase = MatchGS ? MatchGS->GetMatchPhase() : EAscMatchPhase::WaitingForPlayers;

    const bool bLateInitialSelect =
        EffectiveContext == EAscHeroSelectionContext::MatchInitialSelect &&
        Phase == EAscMatchPhase::Playing &&
        (!bHadConfirmedHeroBeforeConfirm || !bHadPawnBeforeConfirm);

    // CharacterSelect stores the confirmation only. PreRoundWaiting spawns all confirmed players.
    // If a player confirms late after Playing already started, spawn immediately using current PawnData.
    const bool bShouldSpawnImmediately =
        EffectiveContext == EAscHeroSelectionContext::InGameRespawnRoom ||
        (EffectiveContext == EAscHeroSelectionContext::MatchInitialSelect && Phase == EAscMatchPhase::PreRoundWaiting) ||
        bLateInitialSelect;

    UE_LOG(LogTemp, Warning,
        TEXT("[HeroSelect] SpawnDecision PC=%s Context=%d Phase=%d HadHero=%d HadPawn=%d LateInitial=%d ShouldSpawn=%d"),
        *GetNameSafe(PlayerController),
        static_cast<int32>(EffectiveContext),
        static_cast<int32>(Phase),
        bHadConfirmedHeroBeforeConfirm ? 1 : 0,
        bHadPawnBeforeConfirm ? 1 : 0,
        bLateInitialSelect ? 1 : 0,
        bShouldSpawnImmediately ? 1 : 0);

    if (bShouldSpawnImmediately && !SpawnOrReplaceHeroPawn(PlayerController))
    {
        if (PlayerController)
        {
            PlayerController->Client_NotifyHeroConfirmResult(false, EAscHeroSelectionRejectReason::InvalidHero);
        }
        return false;
    }

    if (PlayerController)
    {
        PlayerController->Client_NotifyHeroConfirmResult(true, EAscHeroSelectionRejectReason::None);
    }

    return true;
}

EAscHeroSelectionContext AMatchGameMode::ResolveEffectiveHeroSelectionContext(
    AAscenduntPlayerController* PlayerController,
    EAscHeroSelectionContext RequestedContext) const
{
    const AMatchGameState* MatchGS = GetAscMatchGameState();
    const EAscMatchPhase Phase = MatchGS ? MatchGS->GetMatchPhase() : EAscMatchPhase::WaitingForPlayers;

    const AAscenduntPlayerState* AscPS = PlayerController
        ? PlayerController->GetPlayerState<AAscenduntPlayerState>()
        : nullptr;

    // A stale selection widget can survive until death. Dead selection is always
    // stored for the next spawn and must never replace a Pawn immediately.
    if (Phase == EAscMatchPhase::Playing && AscPS && AscPS->IsWaitingForRespawn())
    {
        return EAscHeroSelectionContext::DeadRespawn;
    }

    // A character-selection widget opened during CharacterSelect can survive
    // into Playing. If the player is now inside the respawn room, treat that
    // stale initial-select context as an in-game respawn-room change.
    if (RequestedContext == EAscHeroSelectionContext::MatchInitialSelect &&
        Phase == EAscMatchPhase::Playing &&
        IsPlayerAllowedToUseRespawnRoomSelection(PlayerController))
    {
        return EAscHeroSelectionContext::InGameRespawnRoom;
    }

    return RequestedContext;
}

bool AMatchGameMode::CanPlayerConfirmHero(
    AAscenduntPlayerController* PlayerController,
    int32 HeroId,
    EAscHeroSelectionContext Context,
    EAscHeroSelectionRejectReason& OutRejectReason) const
{
    OutRejectReason = EAscHeroSelectionRejectReason::None;

    const AAscenduntPlayerState* AscPS = PlayerController ? PlayerController->GetPlayerState<AAscenduntPlayerState>() : nullptr;
    if (!PlayerController || !AscPS)
    {
        OutRejectReason = EAscHeroSelectionRejectReason::InvalidPlayer;
        return false;
    }

    const AMatchGameState* MatchGS = GetAscMatchGameState();
    const EAscMatchPhase Phase = MatchGS ? MatchGS->GetMatchPhase() : EAscMatchPhase::WaitingForPlayers;
    const EAscHeroSelectionContext EffectiveContext = ResolveEffectiveHeroSelectionContext(PlayerController, Context);

    if (EffectiveContext == EAscHeroSelectionContext::MatchInitialSelect)
    {
        const bool bHasConfirmedHero = AscPS->HasConfirmedHero() || AscPS->GetPawnData() != nullptr;

        const bool bInitialSelectPhase = Phase == EAscMatchPhase::CharacterSelect ||
            Phase == EAscMatchPhase::PreRoundWaiting;

        const bool bLateInitialSelect = Phase == EAscMatchPhase::Playing && !bHasConfirmedHero;

        if (!bInitialSelectPhase && !bLateInitialSelect)
        {
            OutRejectReason = EAscHeroSelectionRejectReason::InvalidPhase;
            return false;
        }
    }
    else if (EffectiveContext == EAscHeroSelectionContext::InGameRespawnRoom)
    {
        if (!IsPlayerAllowedToUseRespawnRoomSelection(PlayerController))
        {
            OutRejectReason = Phase == EAscMatchPhase::Playing
                ? EAscHeroSelectionRejectReason::NotInRespawnRoom
                : EAscHeroSelectionRejectReason::InvalidPhase;
            return false;
        }
    }
    else if (EffectiveContext == EAscHeroSelectionContext::DeadRespawn)
    {
        if (Phase != EAscMatchPhase::Playing || !AscPS->IsWaitingForRespawn())
        {
            OutRejectReason = Phase == EAscMatchPhase::Playing
                ? EAscHeroSelectionRejectReason::NotWaitingForRespawn
                : EAscHeroSelectionRejectReason::InvalidPhase;
            return false;
        }
    }

    if (!HeroSelectionComponent)
    {
        OutRejectReason = EAscHeroSelectionRejectReason::InvalidHero;
        return false;
    }

    return HeroSelectionComponent->CanConfirmHero(
        PlayerController,
        HeroId,
        PendingMatchSettings.Rules.HeroSelectionRules,
        OutRejectReason);
}

void AMatchGameMode::UpdateMatchStateHeroSelection(AAscenduntPlayerState* PlayerState)
{
    if (!PlayerState)
    {
        return;
    }

    if (AMatchGameState* MatchGS = GetAscMatchGameState())
    {
        MatchGS->SetPlayerHeroSelection(
            PlayerState->GetPlayerId(),
            PlayerState->GetSelectedHeroId(),
            PlayerState->GetConfirmedHeroId(),
            PlayerState->GetConfirmedHeroRole(),
            PlayerState->HasConfirmedHero());
    }
}

bool AMatchGameMode::SpawnOrReplaceHeroPawn(AAscenduntPlayerController* PlayerController)
{
    if (!PlayerController)
    {
        return false;
    }

    AAscenduntPlayerState* AscPS = PlayerController->GetPlayerState<AAscenduntPlayerState>();
    const UHeroPawnData* PawnData = AscPS ? AscPS->GetPawnData() : nullptr;
    if (!AscPS || !PawnData || !PawnData->PawnClass)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[MatchGM] Cannot spawn hero pawn. PC=%s PawnData=%s PawnClass=%s"),
            *GetNameSafe(PlayerController),
            *GetNameSafe(PawnData),
            PawnData ? *GetNameSafe(PawnData->PawnClass) : TEXT("None"));
        return false;
    }

    // Validate a spawn point before tearing down the current Pawn/hero state.
    AActor* TeamStart = ChoosePlayerStart(PlayerController);
    if (!TeamStart)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[MatchGM] Cannot spawn hero pawn. PC=%s Team=%d No PlayerStart found."),
            *GetNameSafe(PlayerController),
            AscPS->GetTeamId());
        return false;
    }

    APawn* OldPawn = PlayerController->GetPawn();
    if (AHeroCharacter* OldHero = Cast<AHeroCharacter>(OldPawn))
    {
        // Normal live hero replacement removes the previous hero AbilitySet. Death
        // respawn has already unpossessed its corpse and handles preserve/change below.
        OldHero->DeinitializeForPawnReplacement(/*bPreserveHeroState*/ false);
    }

    if (OldPawn)
    {
        PlayerController->UnPossess();
        OldPawn->Destroy();
    }

    PlayerController->ChangeState(NAME_Playing);
    RestartPlayerAtPlayerStart(PlayerController, TeamStart);

    APawn* NewPawn = PlayerController->GetPawn();
    const bool bSpawned = IsValid(NewPawn) && NewPawn->IsA(PawnData->PawnClass);
    if (!bSpawned)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[MatchGM] Spawned Pawn does not match PawnData. PC=%s PawnData=%s PawnClass=%s NewPawn=%s"),
            *GetNameSafe(PlayerController),
            *GetNameSafe(PawnData),
            *GetNameSafe(PawnData->PawnClass),
            *GetNameSafe(NewPawn));
        return false;
    }

    // HeroInitComponent owns ASC ActorInfo, PawnData AbilitySet application and
    // GameplayReady progression. GameMode stops at Pawn lifecycle management.
    RefreshRespawnRoomStateForPawn(NewPawn);

    UE_LOG(LogTemp, Warning,
        TEXT("[MatchGM] SpawnOrReplaceHeroPawn PC=%s Team=%d PawnData=%s PawnClass=%s NewPawn=%s Result=1"),
        *GetNameSafe(PlayerController),
        AscPS->GetTeamId(),
        *GetNameSafe(PawnData),
        *GetNameSafe(PawnData->PawnClass),
        *GetNameSafe(NewPawn));

    return true;
}

bool AMatchGameMode::RequestOpenRespawnRoomHeroSelection(AAscenduntPlayerController* PlayerController)
{
    if (!PlayerController)
    {
        return false;
    }

    if (!IsPlayerAllowedToUseRespawnRoomSelection(PlayerController))
    {
        PlayerController->Client_NotifyHeroConfirmResult(false, EAscHeroSelectionRejectReason::NotInRespawnRoom);
        UE_LOG(LogTemp, Verbose,
            TEXT("[HeroSelect] Open respawn-room selection rejected. PC=%s"),
            *GetNameSafe(PlayerController));
        return false;
    }

    PlayerController->Client_OpenHeroSelection(EAscHeroSelectionContext::InGameRespawnRoom);
    return true;
}

bool AMatchGameMode::RequestOpenDeathHeroSelection(AAscenduntPlayerController* PlayerController)
{
    if (!PlayerController || !IsPlayerWaitingForRespawn(PlayerController))
    {
        if (PlayerController)
        {
            PlayerController->Client_NotifyHeroConfirmResult(false, EAscHeroSelectionRejectReason::NotWaitingForRespawn);
        }
        return false;
    }

    PlayerController->Client_OpenHeroSelection(EAscHeroSelectionContext::DeadRespawn);
    return true;
}

void AMatchGameMode::HandleHeroDeath(AHeroCharacter* DeadHero)
{
    if (!HasAuthority() || !IsValid(DeadHero) || bMatchFinalized)
    {
        return;
    }

    AMatchGameState* MatchGS = GetAscMatchGameState();
    if (!MatchGS || MatchGS->GetMatchPhase() != EAscMatchPhase::Playing)
    {
        return;
    }

    AAscenduntPlayerController* PlayerController = Cast<AAscenduntPlayerController>(DeadHero->GetController());
    AAscenduntPlayerState* AscPS = PlayerController
        ? PlayerController->GetPlayerState<AAscenduntPlayerState>()
        : nullptr;
    if (!PlayerController || !AscPS || AscPS->GetRespawnState() != EAscPlayerRespawnState::Alive)
    {
        return;
    }

    // Replace any stale runtime entry without treating death-start as final cleanup.
    // ScheduleRespawnTimer is the single owner of timer replacement.
    FAscPlayerRespawnRuntimeData& Runtime = PlayerRespawnRuntime.FindOrAdd(PlayerController);
    if (Runtime.Corpse.IsValid() && Runtime.Corpse.Get() != DeadHero)
    {
        Runtime.Corpse->Destroy();
    }
    Runtime.Corpse = DeadHero;

    if (UAscenduntAbilitySystemComponent* ASC = AscPS->GetAscenduntAbilitySystemComponent())
    {
        ASC->CancelAllAbilities();
        ASC->ClearAbilityInput();
        RemoveLifecycleEffectsOnDeath(ASC);
    }

    // HeroInitComponent already converted the player to ragdoll before FinishDeath.
    // Do not call SetActorEnableCollision(false): the capsule is disabled by the Hero,
    // while the skeletal mesh must keep QueryAndPhysics collision for the corpse.

    // Preserve the PlayerState-owned hero AbilitySet. Weapon grants are still removed,
    // so the next Pawn receives a fresh default loadout and fresh ammunition.
    DeadHero->DeinitializeForPawnReplacement(/*bPreserveHeroState*/ true);

    // Normalize the persistent PlayerState ASC lifecycle state while it has no AvatarActor,
    // and clear the respawn-room tag so a death inside the room cannot leak permission forward.
    if (UAscenduntAbilitySystemComponent* ASC = AscPS->GetAscenduntAbilitySystemComponent())
    {
        ASC->SetLooseGameplayTagCount(AscenduntGameplayTags::Status_Death_Dying, 0);
        ASC->SetLooseGameplayTagCount(AscenduntGameplayTags::Status_Death_Dead, 1);
        ASC->SetLooseGameplayTagCount(AscenduntGameplayTags::Status_SpawnRoom, 0);
    }

    const float RespawnEndTime = MatchGS->GetServerWorldTimeSeconds() + FMath::Max(0.0f, RespawnDelaySeconds);
    AscPS->SetRespawnState(EAscPlayerRespawnState::DeadWaitingRespawn, RespawnEndTime);

    PlayerController->UnPossess();
    PlayerController->ChangeState(NAME_Spectating);
    PlayerController->Client_EnterDeathSpectating(DeadHero, RespawnEndTime);
    ScheduleRespawnTimer(PlayerController, RespawnDelaySeconds);

    FAscPlayerMatchStats Stats;
    if (MatchGS->GetPlayerStatsByPlayerStateId(AscPS->GetPlayerId(), Stats))
    {
        ++Stats.Deaths;
        MatchGS->UpsertPlayerStats(Stats);
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[Respawn] Death PC=%s Hero=%s PawnData=%s Applied=%s End=%.2f"),
        *GetNameSafe(PlayerController),
        *GetNameSafe(DeadHero),
        *GetNameSafe(AscPS->GetPawnData()),
        *GetNameSafe(AscPS->GetAppliedPawnData()),
        RespawnEndTime);
}

void AMatchGameMode::ScheduleRespawnTimer(AAscenduntPlayerController* PlayerController, float DelaySeconds)
{
    if (!PlayerController || !GetWorld())
    {
        return;
    }

    FAscPlayerRespawnRuntimeData& Runtime = PlayerRespawnRuntime.FindOrAdd(PlayerController);
    GetWorldTimerManager().ClearTimer(Runtime.RespawnTimerHandle);

    FTimerDelegate RespawnDelegate;
    RespawnDelegate.BindUObject(this, &ThisClass::HandleRespawnTimerExpired, PlayerController);
    GetWorldTimerManager().SetTimer(
        Runtime.RespawnTimerHandle,
        RespawnDelegate,
        FMath::Max(0.01f, DelaySeconds),
        false);
}

void AMatchGameMode::HandleRespawnTimerExpired(AAscenduntPlayerController* PlayerController)
{
    AAscenduntPlayerState* AscPS = PlayerController
        ? PlayerController->GetPlayerState<AAscenduntPlayerState>()
        : nullptr;
    AMatchGameState* MatchGS = GetAscMatchGameState();

    if (!PlayerController || !AscPS || !MatchGS || bMatchFinalized ||
        MatchGS->GetMatchPhase() != EAscMatchPhase::Playing || !AscPS->IsWaitingForRespawn())
    {
        return;
    }

    const UHeroPawnData* SelectedPawnData = AscPS->GetPawnData();
    const UHeroPawnData* AppliedPawnData = AscPS->GetAppliedPawnData();
    const UHeroPawnData* RespawnPawnData = SelectedPawnData;

    if (!IsValid(RespawnPawnData) || !RespawnPawnData->PawnClass)
    {
        if (IsValid(AppliedPawnData) && AppliedPawnData->PawnClass)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[Respawn] Invalid selected PawnData. Falling back to applied hero. PC=%s Selected=%s Applied=%s"),
                *GetNameSafe(PlayerController),
                *GetNameSafe(SelectedPawnData),
                *GetNameSafe(AppliedPawnData));

            RespawnPawnData = AppliedPawnData;
            AscPS->SetPawnData(AppliedPawnData);
        }
        else
        {
            UE_LOG(LogTemp, Error,
                TEXT("[Respawn] No valid PawnData. Respawn aborted. PC=%s Selected=%s Applied=%s"),
                *GetNameSafe(PlayerController),
                *GetNameSafe(SelectedPawnData),
                *GetNameSafe(AppliedPawnData));
            ensureMsgf(false, TEXT("Respawn requires either a valid selected PawnData or a valid AppliedPawnData fallback."));
            return;
        }
    }

    AscPS->SetRespawnState(EAscPlayerRespawnState::Respawning);

    UAscenduntAbilitySystemComponent* ASC = AscPS->GetAscenduntAbilitySystemComponent();
    const bool bHeroChanged = AppliedPawnData != nullptr && AppliedPawnData != RespawnPawnData;
    if (bHeroChanged)
    {
        if (ASC)
        {
            RemoveLifecycleEffectsOnHeroChange(ASC);
        }

        // TakeFromAbilitySystem also removes cooldown GEs belonging to removed abilities.
        AscPS->RemovePawnDataAbilitySets();
    }

    // The preserved AbilitySpecs may reactivate OnSpawn abilities as soon as the new
    // AvatarActor is assigned. Clear the dead tags before HeroInitComponent reaches that step.
    if (ASC)
    {
        ASC->SetLooseGameplayTagCount(AscenduntGameplayTags::Status_Death_Dying, 0);
        ASC->SetLooseGameplayTagCount(AscenduntGameplayTags::Status_Death_Dead, 0);
    }

    if (!SpawnOrReplaceHeroPawn(PlayerController))
    {
        if (ASC)
        {
            ASC->SetLooseGameplayTagCount(AscenduntGameplayTags::Status_Death_Dead, 1);
        }

        const float RetryEnd = MatchGS->GetServerWorldTimeSeconds() + RespawnRetryDelaySeconds;
        AscPS->SetRespawnState(EAscPlayerRespawnState::DeadWaitingRespawn, RetryEnd);
        ScheduleRespawnTimer(PlayerController, RespawnRetryDelaySeconds);
        return;
    }

    APawn* NewPawn = PlayerController->GetPawn();
    AscPS->SetRespawnState(EAscPlayerRespawnState::Alive);
    PlayerController->Client_CloseHeroSelection();
    PlayerController->Client_ExitDeathSpectating(NewPawn);
    CleanupRespawnRuntime(PlayerController);

    UE_LOG(LogTemp, Warning,
        TEXT("[Respawn] Complete PC=%s HeroChanged=%d Selected=%s AppliedBefore=%s NewPawn=%s"),
        *GetNameSafe(PlayerController),
        bHeroChanged ? 1 : 0,
        *GetNameSafe(RespawnPawnData),
        *GetNameSafe(AppliedPawnData),
        *GetNameSafe(NewPawn));
}

void AMatchGameMode::CleanupRespawnRuntime(AAscenduntPlayerController* PlayerController)
{
    FAscPlayerRespawnRuntimeData Runtime;
    if (!PlayerController || !PlayerRespawnRuntime.RemoveAndCopyValue(PlayerController, Runtime))
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(Runtime.RespawnTimerHandle);
    if (Runtime.Corpse.IsValid())
    {
        Runtime.Corpse->Destroy();
    }
}

void AMatchGameMode::CleanupAllRespawnRuntime()
{
    for (TPair<AAscenduntPlayerController*, FAscPlayerRespawnRuntimeData>& Pair : PlayerRespawnRuntime)
    {
        GetWorldTimerManager().ClearTimer(Pair.Value.RespawnTimerHandle);
        if (Pair.Value.Corpse.IsValid())
        {
            Pair.Value.Corpse->Destroy();
        }
    }

    PlayerRespawnRuntime.Reset();
}

bool AMatchGameMode::IsPlayerWaitingForRespawn(const AAscenduntPlayerController* PlayerController) const
{
    const AAscenduntPlayerState* AscPS = PlayerController
        ? PlayerController->GetPlayerState<AAscenduntPlayerState>()
        : nullptr;
    const AMatchGameState* MatchGS = GetAscMatchGameState();

    return AscPS && MatchGS && !bMatchFinalized &&
        MatchGS->GetMatchPhase() == EAscMatchPhase::Playing &&
        AscPS->IsWaitingForRespawn();
}

void AMatchGameMode::RemoveLifecycleEffectsOnDeath(UAscenduntAbilitySystemComponent* ASC) const
{
    if (!ASC)
    {
        return;
    }

    FGameplayTagContainer Tag;
    Tag.AddTag(AscenduntGameplayTags::Effect_RemoveOnDeath);
    ASC->RemoveActiveEffectsWithGrantedTags(Tag);
}

void AMatchGameMode::RemoveLifecycleEffectsOnHeroChange(UAscenduntAbilitySystemComponent* ASC) const
{
    if (!ASC)
    {
        return;
    }

    FGameplayTagContainer Tag;
    Tag.AddTag(AscenduntGameplayTags::Effect_RemoveOnDeath);
    Tag.AddTag(AscenduntGameplayTags::Effect_RemoveOnHeroChange);
    ASC->RemoveActiveEffectsWithGrantedTags(Tag);
}

bool AMatchGameMode::IsPlayerAllowedToUseRespawnRoomSelection(AAscenduntPlayerController* PlayerController) const
{
    const AAscenduntPlayerState* AscPS = PlayerController ? PlayerController->GetPlayerState<AAscenduntPlayerState>() : nullptr;
    const AMatchGameState* MatchGS = GetAscMatchGameState();
    const EAscMatchPhase Phase = MatchGS ? MatchGS->GetMatchPhase() : EAscMatchPhase::WaitingForPlayers;

    // PreRoundWaiting is treated as a closed spawn-room phase. This prevents a
    // timing issue where the pawn just spawned but the volume overlap has not
    // fired yet.
    if (Phase == EAscMatchPhase::PreRoundWaiting)
    {
        return true;
    }

    // Training/Practice can reuse this same context and allow immediate selection.
    if (PendingMatchSettings.Rules.Mode == EAscMatchMode::Practice)
    {
        return true;
    }

    // During active play, only the server-authoritative respawn-room volume can
    // re-enable hero selection. Client-side H key/UI is not trusted.
    if (Phase == EAscMatchPhase::Playing)
    {
        const UAscenduntAbilitySystemComponent* ASC = AscPS ? AscPS->GetAscenduntAbilitySystemComponent() : nullptr;
        return ASC && ASC->HasMatchingGameplayTag(AscenduntGameplayTags::Status_SpawnRoom);
    }

    return false;
}

FName AMatchGameMode::GetPlayerStartTagForTeam(uint8 TeamId) const
{
    if (TeamId == 0)
    {
        return NAME_None;
    }

    return FName(*FString::Printf(TEXT("%s%d"), *TeamPlayerStartTagPrefix, static_cast<int32>(TeamId)));
}

bool AMatchGameMode::DoesPlayerStartMatchTeamTag(const APlayerStart* PlayerStart, FName RequiredStartTag) const
{
    if (!PlayerStart || RequiredStartTag.IsNone())
    {
        return false;
    }

    // Support both the dedicated APlayerStart::PlayerStartTag property and the
    // generic Actor Tags array, because level designers often use the latter in Details.
    return PlayerStart->PlayerStartTag == RequiredStartTag || PlayerStart->ActorHasTag(RequiredStartTag);
}

void AMatchGameMode::RefreshRespawnRoomStateForPawn(APawn* Pawn) const
{
    if (!HasAuthority() || !Pawn || !GetWorld())
    {
        return;
    }

    bool bAnyRoomContainsPawn = false;
    for (TActorIterator<AAscRespawnRoomVolume> It(GetWorld()); It; ++It)
    {
        AAscRespawnRoomVolume* RespawnRoom = *It;
        if (IsValid(RespawnRoom) && RespawnRoom->RefreshActorRoomState(Pawn))
        {
            bAnyRoomContainsPawn = true;
        }
    }

    UE_LOG(LogTemp, Verbose,
        TEXT("[MatchGM] RefreshRespawnRoomStateForPawn Pawn=%s InAnyRoom=%d"),
        *GetNameSafe(Pawn),
        bAnyRoomContainsPawn ? 1 : 0);
}

bool AMatchGameMode::IsPlayerStartAvailable(const AActor* PlayerStart, const AController* Player) const
{
    if (!PlayerStart || PlayerStartOccupancyRadius <= 0.0f || !GetWorld())
    {
        return true;
    }

    const FVector StartLocation = PlayerStart->GetActorLocation();
    const float OccupancyRadiusSq = FMath::Square(PlayerStartOccupancyRadius);

    for (TActorIterator<APawn> It(GetWorld()); It; ++It)
    {
        const APawn* ExistingPawn = *It;
        if (!IsValid(ExistingPawn) || ExistingPawn->GetController() == Player)
        {
            continue;
        }

        if (FVector::DistSquared(ExistingPawn->GetActorLocation(), StartLocation) <= OccupancyRadiusSq)
        {
            return false;
        }
    }

    return true;
}

void AMatchGameMode::BeginPhaseCountdown(int32 DurationSeconds)
{
    StopPhaseCountdown();

    CurrentPhaseRemainingSeconds = FMath::Max(0, DurationSeconds);
    SetPhaseRemainingSeconds(CurrentPhaseRemainingSeconds);

    if (CurrentPhaseRemainingSeconds > 0)
    {
        GetWorldTimerManager().SetTimer(
            PhaseCountdownTimerHandle,
            this,
            &ThisClass::TickPhaseCountdown,
            1.0f,
            true);
    }
}

void AMatchGameMode::StopPhaseCountdown()
{
    GetWorldTimerManager().ClearTimer(PhaseCountdownTimerHandle);
}

void AMatchGameMode::TickPhaseCountdown()
{
    CurrentPhaseRemainingSeconds = FMath::Max(0, CurrentPhaseRemainingSeconds - 1);
    SetPhaseRemainingSeconds(CurrentPhaseRemainingSeconds);

    if (CurrentPhaseRemainingSeconds <= 0)
    {
        StopPhaseCountdown();
    }
}

void AMatchGameMode::SetPhaseRemainingSeconds(int32 RemainingSeconds)
{
    if (AMatchGameState* MatchGS = GetAscMatchGameState())
    {
        FAscMatchClockState ClockState = MatchGS->GetClockState();
        ClockState.PhaseRemainingSeconds = FMath::Max(0, RemainingSeconds);
        MatchGS->SetClockState(ClockState);
    }
}

void AMatchGameMode::OpenInitialHeroSelectionForAllPlayers()
{
    if (!GetWorld())
    {
        return;
    }

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (AAscenduntPlayerController* AscPC = Cast<AAscenduntPlayerController>(It->Get()))
        {
            AscPC->Client_OpenHeroSelection(EAscHeroSelectionContext::MatchInitialSelect);
        }
    }
}

void AMatchGameMode::OpenInitialHeroSelectionForUnconfirmedPlayers()
{
    if (!GetWorld())
    {
        return;
    }

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        AAscenduntPlayerController* AscPC = Cast<AAscenduntPlayerController>(It->Get());
        const AAscenduntPlayerState* AscPS = AscPC ? AscPC->GetPlayerState<AAscenduntPlayerState>() : nullptr;
        if (AscPC && (!AscPS || !AscPS->HasConfirmedHero()))
        {
            AscPC->Client_OpenHeroSelection(EAscHeroSelectionContext::MatchInitialSelect);
        }
    }
}

void AMatchGameMode::SpawnConfirmedHeroPawns()
{
    if (!GetWorld())
    {
        return;
    }

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        AAscenduntPlayerController* AscPC = Cast<AAscenduntPlayerController>(It->Get());
        const AAscenduntPlayerState* AscPS = AscPC ? AscPC->GetPlayerState<AAscenduntPlayerState>() : nullptr;
        if (AscPC && AscPS && AscPS->HasConfirmedHero())
        {
            if (SpawnOrReplaceHeroPawn(AscPC))
            {
                AscPC->Client_CloseHeroSelection();
            }
        }
    }
}

uint8 AMatchGameMode::ResolveTeamIdForJoinIndex(int32 JoinIndex) const
{
    if (!PendingMatchSettings.Rules.bTeamBased)
    {
        return static_cast<uint8>(JoinIndex + 1);
    }

    // Temporary assignment until Python sends explicit team assignments.
    // Alternating assignment is more stable when clients arrive in a different order.
    return static_cast<uint8>((JoinIndex % 2) + 1);
}

bool AMatchGameMode::CanAcceptPlayer()
{
    const int32 CurrentPlayers = GameState ? GameState->PlayerArray.Num() : GetNumPlayers();
    return CurrentPlayers < PendingMatchSettings.Rules.MaxPlayers;
}

EAscMatchMode AMatchGameMode::ParseMatchMode(const FString& ModeString, EAscMatchMode FallbackMode)
{
    if (ModeString.IsEmpty())
    {
        return FallbackMode;
    }

    if (ModeString.Equals(TEXT("Duel1v1"), ESearchCase::IgnoreCase) || ModeString.Equals(TEXT("1v1"), ESearchCase::IgnoreCase))
    {
        return EAscMatchMode::Duel1v1;
    }

    if (ModeString.Equals(TEXT("TDM2v2"), ESearchCase::IgnoreCase) || ModeString.Equals(TEXT("2v2"), ESearchCase::IgnoreCase))
    {
        return EAscMatchMode::TDM2v2;
    }

    if (ModeString.Equals(TEXT("Main3v3"), ESearchCase::IgnoreCase) || ModeString.Equals(TEXT("3v3"), ESearchCase::IgnoreCase))
    {
        return EAscMatchMode::Main3v3;
    }

    if (ModeString.Equals(TEXT("Practice"), ESearchCase::IgnoreCase))
    {
        return EAscMatchMode::Practice;
    }

    if (ModeString.Equals(TEXT("FreeForAll"), ESearchCase::IgnoreCase) || ModeString.Equals(TEXT("FFA"), ESearchCase::IgnoreCase))
    {
        return EAscMatchMode::FreeForAll;
    }

    if (ModeString.IsNumeric())
    {
        return static_cast<EAscMatchMode>(FCString::Atoi(*ModeString));
    }

    return FallbackMode;
}

bool AMatchGameMode::ParseBoolOption(const FString& ValueString, bool bFallbackValue)
{
    if (ValueString.IsEmpty())
    {
        return bFallbackValue;
    }

    if (ValueString.Equals(TEXT("true"), ESearchCase::IgnoreCase) || ValueString.Equals(TEXT("yes"), ESearchCase::IgnoreCase))
    {
        return true;
    }

    if (ValueString.Equals(TEXT("false"), ESearchCase::IgnoreCase) || ValueString.Equals(TEXT("no"), ESearchCase::IgnoreCase))
    {
        return false;
    }

    return FCString::Atoi(*ValueString) != 0;
}

FString AMatchGameMode::ParseDecodedOption(const FString& Options, const TCHAR* Key, const FString& FallbackValue)
{
    const FString Value = UGameplayStatics::ParseOption(Options, Key);
    return Value.IsEmpty() ? FallbackValue : FGenericPlatformHttp::UrlDecode(Value);
}

AMatchGameState* AMatchGameMode::GetAscMatchGameState() const
{
    return GetGameState<AMatchGameState>();
}
