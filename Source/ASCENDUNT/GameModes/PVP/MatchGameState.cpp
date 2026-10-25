// Fill out your copyright notice in the Description page of Project Settings.

#include "MatchGameState.h"

#include "ASCENDUNT/Player/AscenduntPlayerState.h"
#include "Net/UnrealNetwork.h"

AMatchGameState::AMatchGameState()
{
    bReplicates = true;
}


void AMatchGameState::AddPlayerState(APlayerState* PlayerState)
{
    Super::AddPlayerState(PlayerState);

    if (AAscenduntPlayerState* AscPlayerState = Cast<AAscenduntPlayerState>(PlayerState))
    {
        AscPlayerState->OnPlayerStateChanged.RemoveAll(this);
        AscPlayerState->OnPlayerStateChanged.AddUObject(this, &ThisClass::HandleRosterPlayerStateChanged);
    }

    OnPlayerRosterChanged.Broadcast();
}

void AMatchGameState::RemovePlayerState(APlayerState* PlayerState)
{
    if (AAscenduntPlayerState* AscPlayerState = Cast<AAscenduntPlayerState>(PlayerState))
    {
        AscPlayerState->OnPlayerStateChanged.RemoveAll(this);
    }

    Super::RemovePlayerState(PlayerState);
    OnPlayerRosterChanged.Broadcast();
}

void AMatchGameState::HandleRosterPlayerStateChanged()
{
    OnPlayerRosterChanged.Broadcast();
}

void AMatchGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ThisClass, MatchSettings);
    DOREPLIFETIME(ThisClass, MatchPhase);
    DOREPLIFETIME(ThisClass, ConnectedPlayerCount);
    DOREPLIFETIME(ThisClass, ClockState);
    DOREPLIFETIME(ThisClass, MatchResult);
    DOREPLIFETIME(ThisClass, TeamStates);
    DOREPLIFETIME(ThisClass, PlayerStats);
}

int32 AMatchGameState::GetTeamScore(uint8 TeamId) const
{
    const FAscTeamMatchState* FoundState = TeamStates.FindByPredicate(
        [TeamId](const FAscTeamMatchState& State)
        {
            return State.TeamId == TeamId;
        });

    return FoundState ? FoundState->Score : 0;
}

bool AMatchGameState::GetPlayerStatsByPlayerStateId(int32 PlayerStateId, FAscPlayerMatchStats& OutStats) const
{
    const FAscPlayerMatchStats* FoundStats = PlayerStats.FindByPredicate(
        [PlayerStateId](const FAscPlayerMatchStats& Stats)
        {
            return Stats.PlayerStateId == PlayerStateId;
        });

    if (!FoundStats)
    {
        return false;
    }

    OutStats = *FoundStats;
    return true;
}

void AMatchGameState::SetMatchSettings(const FAscMatchRuntimeSettings& InSettings)
{
    if (!HasAuthority())
    {
        return;
    }

    MatchSettings = InSettings;
    ForceNetUpdate();
    OnMatchGameStateChanged.Broadcast();
}

void AMatchGameState::SetMatchPhase(EAscMatchPhase InPhase)
{
    if (!HasAuthority() || MatchPhase == InPhase)
    {
        return;
    }

    const EAscMatchPhase OldPhase = MatchPhase;
    MatchPhase = InPhase;
    ForceNetUpdate();
    OnMatchGameStateChanged.Broadcast();
    OnMatchPhaseChanged.Broadcast(OldPhase, MatchPhase);
}

void AMatchGameState::SetConnectedPlayerCount(int32 InCount)
{
    if (!HasAuthority())
    {
        return;
    }

    const int32 ClampedCount = FMath::Max(0, InCount);
    if (ConnectedPlayerCount == ClampedCount)
    {
        return;
    }

    ConnectedPlayerCount = ClampedCount;
    ForceNetUpdate();
    OnMatchGameStateChanged.Broadcast();
}

void AMatchGameState::SetClockState(const FAscMatchClockState& InState)
{
    if (!HasAuthority())
    {
        return;
    }

    ClockState = InState;
    ForceNetUpdate();
    OnMatchGameStateChanged.Broadcast();
}

void AMatchGameState::SetMatchResult(const FAscMatchResultState& InResult)
{
    if (!HasAuthority())
    {
        return;
    }

    MatchResult = InResult;
    ForceNetUpdate();
    OnMatchGameStateChanged.Broadcast();
    OnMatchResultChanged.Broadcast(MatchResult);
}

void AMatchGameState::SetPostMatchStage(EAscPostMatchStage InStage, float StageEndServerTimeSeconds)
{
    if (!HasAuthority())
    {
        return;
    }

    MatchResult.PostMatchStage = InStage;
    MatchResult.StageEndServerTimeSeconds = FMath::Max(0.0f, StageEndServerTimeSeconds);
    ForceNetUpdate();
    OnMatchGameStateChanged.Broadcast();
    OnMatchResultChanged.Broadcast(MatchResult);
}

void AMatchGameState::SetTeamState(const FAscTeamMatchState& InState)
{
    if (!HasAuthority())
    {
        return;
    }

    if (FAscTeamMatchState* ExistingState = TeamStates.FindByPredicate(
        [&InState](const FAscTeamMatchState& State)
        {
            return State.TeamId == InState.TeamId;
        }))
    {
        *ExistingState = InState;
    }
    else
    {
        TeamStates.Add(InState);
    }

    ForceNetUpdate();
    OnMatchGameStateChanged.Broadcast();
}

void AMatchGameState::SetTeamScore(uint8 TeamId, int32 InScore)
{
    if (!HasAuthority())
    {
        return;
    }

    FAscTeamMatchState* ExistingState = TeamStates.FindByPredicate(
        [TeamId](const FAscTeamMatchState& State)
        {
            return State.TeamId == TeamId;
        });

    if (ExistingState)
    {
        if (ExistingState->Score == InScore)
        {
            return;
        }

        ExistingState->Score = InScore;
    }
    else
    {
        FAscTeamMatchState NewState;
        NewState.TeamId = TeamId;
        NewState.Score = InScore;
        TeamStates.Add(NewState);
    }

    ForceNetUpdate();
    OnMatchGameStateChanged.Broadcast();
}

void AMatchGameState::UpsertPlayerStats(const FAscPlayerMatchStats& InStats)
{
    if (!HasAuthority())
    {
        return;
    }

    if (FAscPlayerMatchStats* ExistingStats = PlayerStats.FindByPredicate(
        [&InStats](const FAscPlayerMatchStats& Stats)
        {
            return Stats.PlayerStateId == InStats.PlayerStateId;
        }))
    {
        *ExistingStats = InStats;
    }
    else
    {
        PlayerStats.Add(InStats);
    }

    ForceNetUpdate();
    OnMatchGameStateChanged.Broadcast();
}

void AMatchGameState::SetPlayerHeroSelection(
    int32 PlayerStateId,
    int32 SelectedHeroId,
    int32 ConfirmedHeroId,
    EAscHeroRole ConfirmedHeroRole,
    bool bHeroConfirmed)
{
    if (!HasAuthority())
    {
        return;
    }

    FAscPlayerMatchStats* ExistingStats = PlayerStats.FindByPredicate(
        [PlayerStateId](const FAscPlayerMatchStats& Stats)
        {
            return Stats.PlayerStateId == PlayerStateId;
        });

    if (!ExistingStats)
    {
        FAscPlayerMatchStats NewStats;
        NewStats.PlayerStateId = PlayerStateId;
        PlayerStats.Add(NewStats);
        ExistingStats = &PlayerStats.Last();
    }

    ExistingStats->SelectedHeroId = SelectedHeroId;
    ExistingStats->ConfirmedHeroId = ConfirmedHeroId;
    ExistingStats->ConfirmedHeroRole = ConfirmedHeroRole;
    ExistingStats->bHeroConfirmed = bHeroConfirmed;

    ForceNetUpdate();
    OnMatchGameStateChanged.Broadcast();
}

void AMatchGameState::MarkPlayerDisconnected(int32 PlayerStateId)
{
    if (!HasAuthority())
    {
        return;
    }

    if (FAscPlayerMatchStats* ExistingStats = PlayerStats.FindByPredicate(
        [PlayerStateId](const FAscPlayerMatchStats& Stats)
        {
            return Stats.PlayerStateId == PlayerStateId;
        }))
    {
        ExistingStats->bConnected = false;
        ForceNetUpdate();
        OnMatchGameStateChanged.Broadcast();
    }
}

void AMatchGameState::OnRep_MatchSettings()
{
    OnMatchGameStateChanged.Broadcast();
}

void AMatchGameState::OnRep_MatchPhase(EAscMatchPhase OldPhase)
{
    OnMatchGameStateChanged.Broadcast();
    OnMatchPhaseChanged.Broadcast(OldPhase, MatchPhase);
}

void AMatchGameState::OnRep_ConnectedPlayerCount()
{
    OnMatchGameStateChanged.Broadcast();
}

void AMatchGameState::OnRep_ClockState()
{
    OnMatchGameStateChanged.Broadcast();
}

void AMatchGameState::OnRep_MatchResult()
{
    OnMatchGameStateChanged.Broadcast();
    OnMatchResultChanged.Broadcast(MatchResult);
}

void AMatchGameState::OnRep_TeamStates()
{
    OnMatchGameStateChanged.Broadcast();
}

void AMatchGameState::OnRep_PlayerStats()
{
    OnMatchGameStateChanged.Broadcast();
}
