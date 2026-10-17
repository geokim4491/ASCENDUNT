// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "ASCENDUNT/Match/AscMatchTypes.h"
#include "MatchGameState.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnAscMatchGameStateChangedNative);
DECLARE_MULTICAST_DELEGATE(FOnAscPlayerRosterChangedNative);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAscMatchPhaseChanged, EAscMatchPhase, OldPhase, EAscMatchPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAscMatchResultChanged, FAscMatchResultState, MatchResult);

/**
 * Replicated match runtime state.
 *
 * GameMode owns the rules and state transitions, but clients cannot access
 * GameMode. Anything the HUD, scoreboard, map intro, character select, or
 * allied ultimate UI needs should be replicated through this GameState.
 */
UCLASS()
class ASCENDUNT_API AMatchGameState : public AGameState
{
    GENERATED_BODY()

public:
    AMatchGameState();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void AddPlayerState(APlayerState* PlayerState) override;
    virtual void RemovePlayerState(APlayerState* PlayerState) override;

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Match")
    const FAscMatchRuntimeSettings& GetMatchSettings() const { return MatchSettings; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Match")
    const FAscMatchRuleSettings& GetRules() const { return MatchSettings.Rules; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Match")
    EAscMatchMode GetMatchMode() const { return MatchSettings.Rules.Mode; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Match")
    EAscMatchPhase GetMatchPhase() const { return MatchPhase; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Match")
    bool IsTeamBasedMode() const { return MatchSettings.Rules.bTeamBased; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Match")
    int32 GetConnectedPlayerCount() const { return ConnectedPlayerCount; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Match")
    int32 GetRequiredPlayerCount() const { return MatchSettings.Rules.RequiredPlayers; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Match")
    const FAscMatchClockState& GetClockState() const { return ClockState; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Match")
    const TArray<FAscTeamMatchState>& GetTeamStates() const { return TeamStates; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Match")
    const TArray<FAscPlayerMatchStats>& GetPlayerStats() const { return PlayerStats; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Match")
    const FAscMatchResultState& GetMatchResult() const { return MatchResult; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Match")
    EAscMatchResult GetTeamMatchResult(uint8 TeamId) const { return MatchResult.GetResultForTeam(TeamId); }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Match")
    int32 GetTeamScore(uint8 TeamId) const;

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Match")
    bool GetPlayerStatsByPlayerStateId(int32 PlayerStateId, FAscPlayerMatchStats& OutStats) const;

    void SetMatchSettings(const FAscMatchRuntimeSettings& InSettings);
    void SetMatchPhase(EAscMatchPhase InPhase);
    void SetConnectedPlayerCount(int32 InCount);
    void SetClockState(const FAscMatchClockState& InState);
    void SetMatchResult(const FAscMatchResultState& InResult);
    void SetPostMatchStage(EAscPostMatchStage InStage, float StageEndServerTimeSeconds);
    void SetTeamState(const FAscTeamMatchState& InState);
    void SetTeamScore(uint8 TeamId, int32 InScore);
    void UpsertPlayerStats(const FAscPlayerMatchStats& InStats);
    void SetPlayerHeroSelection(int32 PlayerStateId, int32 SelectedHeroId, int32 ConfirmedHeroId, EAscHeroRole ConfirmedHeroRole, bool bHeroConfirmed);
    void MarkPlayerDisconnected(int32 PlayerStateId);

    FOnAscMatchGameStateChangedNative OnMatchGameStateChanged;
    FOnAscPlayerRosterChangedNative OnPlayerRosterChanged;

    /** Fired on the server from SetMatchPhase() and on clients from OnRep_MatchPhase(). */
    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Match")
    FOnAscMatchPhaseChanged OnMatchPhaseChanged;

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Match")
    FOnAscMatchResultChanged OnMatchResultChanged;

protected:
    void HandleRosterPlayerStateChanged();

    UFUNCTION()
    void OnRep_MatchSettings();

    UFUNCTION()
    void OnRep_MatchPhase(EAscMatchPhase OldPhase);

    UFUNCTION()
    void OnRep_ConnectedPlayerCount();

    UFUNCTION()
    void OnRep_ClockState();

    UFUNCTION()
    void OnRep_MatchResult();

    UFUNCTION()
    void OnRep_TeamStates();

    UFUNCTION()
    void OnRep_PlayerStats();

protected:
    UPROPERTY(ReplicatedUsing = OnRep_MatchSettings, BlueprintReadOnly, Category = "Ascendunt|Match")
    FAscMatchRuntimeSettings MatchSettings;

    UPROPERTY(ReplicatedUsing = OnRep_MatchPhase, BlueprintReadOnly, Category = "Ascendunt|Match")
    EAscMatchPhase MatchPhase = EAscMatchPhase::WaitingForPlayers;

    UPROPERTY(ReplicatedUsing = OnRep_ConnectedPlayerCount, BlueprintReadOnly, Category = "Ascendunt|Match")
    int32 ConnectedPlayerCount = 0;

    UPROPERTY(ReplicatedUsing = OnRep_ClockState, BlueprintReadOnly, Category = "Ascendunt|Match")
    FAscMatchClockState ClockState;

    UPROPERTY(ReplicatedUsing = OnRep_MatchResult, BlueprintReadOnly, Category = "Ascendunt|Match")
    FAscMatchResultState MatchResult;

    UPROPERTY(ReplicatedUsing = OnRep_TeamStates, BlueprintReadOnly, Category = "Ascendunt|Match")
    TArray<FAscTeamMatchState> TeamStates;

    UPROPERTY(ReplicatedUsing = OnRep_PlayerStats, BlueprintReadOnly, Category = "Ascendunt|Match")
    TArray<FAscPlayerMatchStats> PlayerStats;
};
