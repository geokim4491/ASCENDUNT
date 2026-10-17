#pragma once

#include "CoreMinimal.h"

class UHeroPawnData;
class UTexture2D;
class UAscHeroHUDPanelWidget;

#include "AscMatchTypes.generated.h"

/**
 * Match mode selected by the Python orchestrator / client quick-match UI.
 *
 * This replaces the old room/session-oriented mode enum.
 */
UENUM(BlueprintType)
enum class EAscMatchMode : uint8
{
    Duel1v1       UMETA(DisplayName = "1v1 Duel"),
    TDM2v2        UMETA(DisplayName = "2v2 Team Deathmatch"),
    Main3v3       UMETA(DisplayName = "3v3 Main Mode"),
    Practice      UMETA(DisplayName = "Practice"),
    FreeForAll    UMETA(DisplayName = "Free For All"),
};

/** Client-side queue/ticket status returned by the Python orchestrator. */
UENUM(BlueprintType)
enum class EAscQueueTicketStatus : uint8
{
    None       UMETA(DisplayName = "None"),
    Queued     UMETA(DisplayName = "Queued"),
    Launching  UMETA(DisplayName = "Launching"),
    Ready      UMETA(DisplayName = "Ready"),
    Cancelled  UMETA(DisplayName = "Cancelled"),
    Failed     UMETA(DisplayName = "Failed"),
    Expired    UMETA(DisplayName = "Expired"),
};

/** Server-side match flow phase replicated through AMatchGameState. */
UENUM(BlueprintType)
enum class EAscMatchPhase : uint8
{
    WaitingForPlayers  UMETA(DisplayName = "Waiting For Players"),
    MapIntro           UMETA(DisplayName = "Map Intro"),
    CharacterSelect    UMETA(DisplayName = "Character Select"),
    PreRoundWaiting    UMETA(DisplayName = "Pre Round Waiting"),
    Playing            UMETA(DisplayName = "Playing"),
    PostMatch          UMETA(DisplayName = "Post Match"),
    Returning          UMETA(DisplayName = "Returning"),
};

/** Why the authoritative match flow ended. */
UENUM(BlueprintType)
enum class EAscMatchEndReason : uint8
{
    ScoreLimitReached  UMETA(DisplayName = "Score Limit Reached"),
    TimeLimitExpired   UMETA(DisplayName = "Time Limit Expired"),
    Error              UMETA(DisplayName = "Error"),
};

/** Result from one team's point of view. */
UENUM(BlueprintType)
enum class EAscMatchResult : uint8
{
    Victory  UMETA(DisplayName = "Victory"),
    Draw     UMETA(DisplayName = "Draw"),
    Defeat   UMETA(DisplayName = "Defeat"),
};

/** Replicated sub-stage while MatchPhase is PostMatch/Returning. */
UENUM(BlueprintType)
enum class EAscPostMatchStage : uint8
{
    None              UMETA(DisplayName = "None"),
    ResultSlowMotion  UMETA(DisplayName = "Result Slow Motion"),
    WinnerShowcase    UMETA(DisplayName = "Winner Showcase"),
    Highlight         UMETA(DisplayName = "Highlight"),
    Returning         UMETA(DisplayName = "Returning"),
};

/**
 * Context of a hero-selection request.
 *
 * Training range can reuse InGameRespawnRoom: enter the map, immediately show
 * the same respawn-room hero selection UI, and apply the confirmed hero.
 */
UENUM(BlueprintType)
enum class EAscHeroSelectionContext : uint8
{
    MatchInitialSelect  UMETA(DisplayName = "Match Initial Select"),
    InGameRespawnRoom  UMETA(DisplayName = "In Game Respawn Room"),
    DeadRespawn        UMETA(DisplayName = "Dead Respawn"),
};

/** Server-authoritative lifecycle state used by respawn, UI countdown, and hero-change validation. */
UENUM(BlueprintType)
enum class EAscPlayerRespawnState : uint8
{
    Alive               UMETA(DisplayName = "Alive"),
    DeadWaitingRespawn  UMETA(DisplayName = "Dead Waiting Respawn"),
    Respawning          UMETA(DisplayName = "Respawning"),
    /** PvE full death: the player remains in spectator mode for the rest of the mission. */
    DeadSpectating      UMETA(DisplayName = "Dead Spectating"),
};

/** Coarse role bucket used by mode-specific duplicate/role lock rules. */
UENUM(BlueprintType)
enum class EAscHeroRole : uint8
{
    SubDPS   UMETA(DisplayName = "SubDPS"),
    MainDPS  UMETA(DisplayName = "MainDPS"),
    Supporter  UMETA(DisplayName = "Supporter")
};

/** Server-side reason returned when a hero confirmation is rejected. */
UENUM(BlueprintType)
enum class EAscHeroSelectionRejectReason : uint8
{
    None                    UMETA(DisplayName = "None"),
    InvalidPlayer           UMETA(DisplayName = "Invalid Player"),
    InvalidHero             UMETA(DisplayName = "Invalid Hero"),
    InvalidPhase            UMETA(DisplayName = "Invalid Phase"),
    NotInRespawnRoom        UMETA(DisplayName = "Not In Respawn Room"),
    NotWaitingForRespawn    UMETA(DisplayName = "Not Waiting For Respawn"),
    DuplicateHeroBlocked    UMETA(DisplayName = "Duplicate Hero Blocked"),
    DuplicateRoleBlocked    UMETA(DisplayName = "Duplicate Role Blocked"),
};

/**
 * Mode-level hero selection rules.
 *
 * Some modes may allow duplicate heroes, while other modes may block duplicate
 * heroes or duplicate roles. These values are intentionally part of the match
 * rules so Python/URL options or future data assets can drive them per mode.
 */
USTRUCT(BlueprintType)
struct FAscHeroSelectionRules
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Hero Selection")
    bool bAllowDuplicateHeroes = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Hero Selection")
    bool bAllowDuplicateHeroRoles = true;

    /**
     * If true, duplicate hero/role rules are checked only against players on the
     * same team. This fits team games such as 3v3 while still allowing FFA-style
     * modes to set this false later if needed.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Hero Selection")
    bool bCheckDuplicateRulesWithinTeamOnly = true;
};

/** Hero id / role / PawnData entry used by server validation. */
USTRUCT(BlueprintType)
struct FAscHeroDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Hero Selection")
    int32 HeroId = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Hero Selection")
    FText HeroName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Hero Selection")
    EAscHeroRole HeroRole = EAscHeroRole::MainDPS;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Hero Selection")
    TObjectPtr<UTexture2D> Portrait = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Hero Selection")
    TObjectPtr<const UHeroPawnData> PawnData = nullptr;

    /** Optional hero-specific panel inserted into the shared in-game HUD. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Hero Selection|UI")
    TSubclassOf<UAscHeroHUDPanelWidget> HeroHUDPanelClass;
};

/** Rule/config values decided before the dedicated server is launched. */
USTRUCT(BlueprintType)
struct FAscMatchRuleSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    EAscMatchMode Mode = EAscMatchMode::Main3v3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match", meta = (ClampMin = "1"))
    int32 RequiredPlayers = 6;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match", meta = (ClampMin = "1"))
    int32 MaxPlayers = 6;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match", meta = (ClampMin = "1"))
    int32 TeamSize = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match", meta = (ClampMin = "1"))
    int32 ScoreLimit = 30;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match", meta = (ClampMin = "0"))
    int32 TimeLimitSeconds = 600;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match", meta = (ClampMin = "0"))
    int32 MapIntroSeconds = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match", meta = (ClampMin = "0"))
    int32 CharacterSelectSeconds = 5;

    // Character selection이 끝난 뒤 스폰룸 문이 닫힌 상태로 기다리는 시간.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match", meta = (ClampMin = "0"))
    int32 PreRoundWaitingSeconds = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    bool bTeamBased = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    FAscHeroSelectionRules HeroSelectionRules;
};


/**
 * GameMode/BP에서 직접 튜닝할 매치 룰.
 * RequiredPlayers / MaxPlayers / TeamSize / bTeamBased는 Mode 프리셋 또는 URL 옵션이 결정한다.
 */
USTRUCT(BlueprintType)
struct FAscMatchGameModeRuleSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    EAscMatchMode Mode = EAscMatchMode::Duel1v1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match", meta = (ClampMin = "0"))
    int32 ScoreLimit = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match", meta = (ClampMin = "0"))
    int32 TimeLimitSeconds = 600;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match", meta = (ClampMin = "0"))
    int32 MapIntroSeconds = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match", meta = (ClampMin = "0"))
    int32 CharacterSelectSeconds = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match", meta = (ClampMin = "0"))
    int32 PreRoundWaitingSeconds = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    FAscHeroSelectionRules HeroSelectionRules;
};

/** Runtime metadata of one dedicated server instance. */
USTRUCT(BlueprintType)
struct FAscMatchRuntimeSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    FString MatchId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    FString MatchName = TEXT("Quick Match");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    FString SelectedMapPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    FString SelectedMapDisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    FAscMatchRuleSettings Rules;
};

/** Match timer values shown by HUD / scoreboard. */
USTRUCT(BlueprintType)
struct FAscMatchClockState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    int32 ElapsedTimeSeconds = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    int32 RemainingTimeSeconds = 0;

    // 현재 MatchPhase가 끝나기까지 남은 시간. CharacterSelect/PreRoundWaiting UI에서 사용한다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    int32 PhaseRemainingSeconds = 0;
};


/** Finalized result and the current post-match presentation stage. */
USTRUCT(BlueprintType)
struct FAscMatchResultState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    bool bFinalized = false;

    // Meaningful only when bFinalized is true.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    EAscMatchEndReason EndReason = EAscMatchEndReason::Error;

    // 0 means draw/no winner. Team result is resolved relative to this id.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    uint8 WinningTeamId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    EAscPostMatchStage PostMatchStage = EAscPostMatchStage::None;

    // Server world time at which the current presentation stage should end.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    float StageEndServerTimeSeconds = 0.0f;

    EAscMatchResult GetResultForTeam(uint8 TeamId) const
    {
        if (!bFinalized || WinningTeamId == 0 || TeamId == 0)
        {
            return EAscMatchResult::Draw;
        }

        return TeamId == WinningTeamId
            ? EAscMatchResult::Victory
            : EAscMatchResult::Defeat;
    }
};

/** Team score and team-level state. */
USTRUCT(BlueprintType)
struct FAscTeamMatchState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    uint8 TeamId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    int32 Score = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    int32 ConnectedPlayers = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    int32 AlivePlayers = 0;
};

/** Per-player values used by the scoreboard and allied ultimate-status UI. */
USTRUCT(BlueprintType)
struct FAscPlayerMatchStats
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    int32 PlayerStateId = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    FString PlayerId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    FString TicketId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    uint8 TeamId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    int32 SelectedHeroId = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    int32 ConfirmedHeroId = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    EAscHeroRole ConfirmedHeroRole = EAscHeroRole::MainDPS;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    bool bHeroConfirmed = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    int32 Kills = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    int32 Deaths = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    int32 Assists = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    int32 DamageDealt = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    int32 DamageTaken = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    int32 HealingDone = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    int32 ObjectiveScore = 0;

    /** 0.0 ~ 1.0. Allied HUD can read this for ultimate-charge display. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float UltimateChargeNormalized = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    bool bUltimateReady = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Match")
    bool bConnected = true;
};

/** Dedicated-server connection info assigned by the Python orchestrator. */
USTRUCT(BlueprintType)
struct FAscMatchServerConnectionInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Queue")
    FString MatchId;

    /** Full client travel address, for example 211.195.87.115:7777. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Queue")
    FString MatchServerTravelAddress;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Queue")
    FString Ip;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Queue")
    int32 Port = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Queue")
    FString SelectedMapPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Queue")
    FString SelectedMapDisplayName;

    bool IsValid() const
    {
        return !MatchServerTravelAddress.IsEmpty() || (!Ip.IsEmpty() && Port > 0);
    }
};

/** Client-side quick-match queue state. It is owned by UAscQuickMatchSubsystem and shown by UI. */
USTRUCT(BlueprintType)
struct FAscClientQueueState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Queue")
    EAscQueueTicketStatus Status = EAscQueueTicketStatus::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Queue")
    EAscMatchMode Mode = EAscMatchMode::Main3v3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Queue")
    FString TicketId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Queue")
    FString PlayerId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Queue")
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Queue")
    int32 CurrentPlayers = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Queue")
    int32 RequiredPlayers = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Queue")
    int32 QueuePosition = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Queue")
    FString ErrorMessage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Queue")
    FAscMatchServerConnectionInfo Connection;

    bool IsWaiting() const
    {
        return Status == EAscQueueTicketStatus::Queued || Status == EAscQueueTicketStatus::Launching;
    }

    bool CanTravelToMatch() const
    {
        return Status == EAscQueueTicketStatus::Ready && Connection.IsValid();
    }
};
