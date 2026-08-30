// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "TimerManager.h"
#include "ASCENDUNT/Match/AscMatchTypes.h"
#include "ASCENDUNT/HeroSelection/AscHeroSelectionAuthorityInterface.h"
#include "MatchGameMode.generated.h"

class AMatchGameState;
class AAscenduntPlayerController;
class AAscenduntPlayerState;
class UAscHeroSelectionComponent;
class UHeroPawnData;
class UWeaponEquipmentComponent;
class UAscenduntAbilitySystemComponent;
class AAscRespawnRoomVolume;
class AHeroCharacter;
class APlayerStart;

struct FAscPlayerRespawnRuntimeData
{
    TWeakObjectPtr<AHeroCharacter> Corpse;
    FTimerHandle RespawnTimerHandle;
};

/**
 * Dedicated-server match GameMode for the new quick-match flow.
 *
 * This version is intentionally independent from the removed Session/Lobby files.
 * It only parses match options, controls connection admission, assigns basic
 * teams, seeds MatchGameState, and exposes phase-transition entry points.
 */
UCLASS()
class ASCENDUNT_API AMatchGameMode : public AGameMode, public IAscHeroSelectionAuthorityInterface
{
    GENERATED_BODY()

public:
    AMatchGameMode();

    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void InitGameState() override;
    virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
    virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
    virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Match")
    const FAscMatchRuntimeSettings& GetResolvedMatchSettings() const { return PendingMatchSettings; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Match")
    EAscMatchMode GetResolvedMatchMode() const { return PendingMatchSettings.Rules.Mode; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Match")
    bool IsTeamBasedMode() const { return PendingMatchSettings.Rules.bTeamBased; }

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Match")
    void StartMapIntroPhase();

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Match")
    void StartCharacterSelectPhase();

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Match")
    void StartPreRoundWaitingPhase();

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Match")
    void StartPlayingPhase();

    /**
     * Common team-score entry point. Derived modes decide when a kill, boss,
     * objective, etc. should call this function.
     */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Match")
    bool AddTeamScore(uint8 TeamId, int32 ScoreDelta = 1);

    /** Ends the current match as an error/no-winner result. */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Match")
    void FinishMatch();

    /** IAscHeroSelectionAuthorityInterface entry point used by PlayerController. */
    virtual bool HandleHeroConfirmRequest(AAscenduntPlayerController* PlayerController, int32 HeroId, EAscHeroSelectionContext Context) override;

    /**
     * Server-authoritative hero confirmation entry point.
     * Character-select UI should call PlayerController::Server_RequestConfirmHero,
     * and the controller delegates to this GameMode through IAscHeroSelectionAuthorityInterface.
     */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Hero Selection")
    bool HandlePlayerHeroConfirmed(AAscenduntPlayerController* PlayerController, int32 HeroId, EAscHeroSelectionContext Context);

    bool CanPlayerConfirmHero(AAscenduntPlayerController* PlayerController, int32 HeroId, EAscHeroSelectionContext Context, EAscHeroSelectionRejectReason& OutRejectReason) const;

    /** Resolves stale UI context into the currently valid server-side selection context. */
    EAscHeroSelectionContext ResolveEffectiveHeroSelectionContext(AAscenduntPlayerController* PlayerController, EAscHeroSelectionContext RequestedContext) const;

    /** Spawns or replaces the player pawn from the PawnData confirmed by hero selection. */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Hero Selection")
    bool SpawnOrReplaceHeroPawn(AAscenduntPlayerController* PlayerController);

    /** Server-validated request used by H-key/in-game respawn room hero selection. */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Hero Selection")
    bool RequestOpenRespawnRoomHeroSelection(AAscenduntPlayerController* PlayerController);

    /** Called by the authoritative HeroCharacter when HealthComponent finishes death. */
    void HandleHeroDeath(AHeroCharacter* DeadHero);

    /** Server-validated H-key request while the player is waiting to respawn. */
    bool RequestOpenDeathHeroSelection(AAscenduntPlayerController* PlayerController);

protected:
    /**
     * BP_GM에서 수정할 매치 튜닝값.
     * RequiredPlayers / MaxPlayers / TeamSize / bTeamBased는 Mode 프리셋 또는 URL 옵션으로 결정한다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ascendunt|Match|Rules")
    FAscMatchGameModeRuleSettings GameModeMatchRules;

    UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|Match|Post Match")
    FString ReturnMainMenuMapPath = TEXT("/Game/Maps/L_MainMenu");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Match|Post Match", meta = (ClampMin = "0.0"))
    float ResultSlowMotionDurationSeconds = 3.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Match|Post Match", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float ResultSlowMotionScale = 0.15f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Match|Post Match", meta = (ClampMin = "0.0"))
    float WinnerShowcaseDurationSeconds = 6.0f;

    /** Reserved for the later highlight implementation. Zero skips this stage. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Match|Post Match", meta = (ClampMin = "0.0"))
    float HighlightDurationSeconds = 0.0f;

    /**
     * Reusable hero selection implementation. Configure its HeroRosterData on
     * BP_MatchGameMode. TrainingGameMode can use the same component class and
     * roster instead of duplicating confirm/validation code.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ascendunt|Hero Selection")
    TObjectPtr<UAscHeroSelectionComponent> HeroSelectionComponent;

    /** PlayerStartTag convention. PlayerState TeamId 1 uses Team1, TeamId 2 uses Team2. */

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Spawning")
    FString TeamPlayerStartTagPrefix = TEXT("Team");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Spawning", meta = (ClampMin = "0"))
    float PlayerStartOccupancyRadius = 160.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Respawn", meta = (ClampMin = "0.0"))
    float RespawnDelaySeconds = 10.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Respawn", meta = (ClampMin = "0.1"))
    float RespawnRetryDelaySeconds = 1.0f;

private:
    UPROPERTY()
    FAscMatchRuntimeSettings PendingMatchSettings;

    /** Stores raw URL options from InitNewPlayer until PostLogin can apply them to PlayerState. */
    TMap<APlayerController*, FString> PendingPlayerOptions;

private:
    void BuildMatchSettingsFromOptions(const FString& MapName, const FString& Options);
    void InitializeDefaultTeamStates();
    void UpdateConnectedPlayerCount();
    void RegisterPlayerInMatchState(APlayerController* NewPlayer, const FString& Options);
    uint8 ResolveTeamIdForJoinIndex(int32 JoinIndex) const;
    bool CanAcceptPlayer();

    void UpdateMatchStateHeroSelection(AAscenduntPlayerState* PlayerState);
    bool IsPlayerAllowedToUseRespawnRoomSelection(AAscenduntPlayerController* PlayerController) const;
    FName GetPlayerStartTagForTeam(uint8 TeamId) const;
    bool IsPlayerStartAvailable(const AActor* PlayerStart, const AController* Player) const;
    bool DoesPlayerStartMatchTeamTag(const class APlayerStart* PlayerStart, FName RequiredStartTag) const;
    void RefreshRespawnRoomStateForPawn(APawn* Pawn) const;
    void HandleRespawnTimerExpired(AAscenduntPlayerController* PlayerController);
    void ScheduleRespawnTimer(AAscenduntPlayerController* PlayerController, float DelaySeconds);
    void CleanupRespawnRuntime(AAscenduntPlayerController* PlayerController);
    void CleanupAllRespawnRuntime();
    bool IsPlayerWaitingForRespawn(const AAscenduntPlayerController* PlayerController) const;
    void RemoveLifecycleEffectsOnDeath(UAscenduntAbilitySystemComponent* ASC) const;
    void RemoveLifecycleEffectsOnHeroChange(UAscenduntAbilitySystemComponent* ASC) const;

    void BeginPhaseCountdown(int32 DurationSeconds);
    void StopPhaseCountdown();
    void TickPhaseCountdown();

    void StartMatchClock();
    void StopMatchClock();
    void TickMatchClock();
    bool TryFinalizeMatch(EAscMatchEndReason EndReason);
    uint8 ResolveWinningTeamId() const;

    void StartResultSlowMotionStage();
    void StartWinnerShowcaseStage();
    void StartHighlightStage();
    void StartReturningStage();
    void SetPostMatchStage(EAscPostMatchStage NewStage, float DurationSeconds);
    void SetGameplayBlockedForAllPlayers(bool bBlocked);
    void SetPhaseRemainingSeconds(int32 RemainingSeconds);
    void OpenInitialHeroSelectionForAllPlayers();
    void OpenInitialHeroSelectionForUnconfirmedPlayers();
    void SpawnConfirmedHeroPawns();

    FTimerHandle PhaseCountdownTimerHandle;
    FTimerHandle PhaseTransitionTimerHandle;
    FTimerHandle MatchClockTimerHandle;
    FTimerHandle PostMatchStageTimerHandle;

    TMap<AAscenduntPlayerController*, FAscPlayerRespawnRuntimeData> PlayerRespawnRuntime;

    /** Stable assignment counter independent from PlayerArray/replication timing. */
    int32 NextJoinIndexForTeamAssignment = 0;

    UPROPERTY()
    int32 CurrentPhaseRemainingSeconds = 0;

    UPROPERTY()
    int32 CurrentMatchElapsedSeconds = 0;

    bool bMatchFinalized = false;

    static EAscMatchMode ParseMatchMode(const FString& ModeString, EAscMatchMode FallbackMode);
    static bool ParseBoolOption(const FString& ValueString, bool bFallbackValue);
    static FString ParseDecodedOption(const FString& Options, const TCHAR* Key, const FString& FallbackValue);

    AMatchGameState* GetAscMatchGameState() const;
};
