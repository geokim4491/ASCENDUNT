#include "AscPvEGameState.h"

#include "Net/UnrealNetwork.h"

void AAscPvEGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AAscPvEGameState, GamePhase);
}

void AAscPvEGameState::SetGamePhase(EAscPvEGamePhase NewPhase)
{
    if (!HasAuthority() || GamePhase == NewPhase)
    {
        return;
    }

    const EAscPvEGamePhase OldPhase = GamePhase;
    GamePhase = NewPhase;

    ForceNetUpdate();
    OnGamePhaseChanged.Broadcast(OldPhase, GamePhase);

    UE_LOG(LogTemp, Log,
        TEXT("[PvEGS] GamePhase changed %d -> %d"),
        static_cast<int32>(OldPhase),
        static_cast<int32>(GamePhase));
}

void AAscPvEGameState::OnRep_GamePhase(EAscPvEGamePhase OldPhase)
{
    OnGamePhaseChanged.Broadcast(OldPhase, GamePhase);

    UE_LOG(LogTemp, Log,
        TEXT("[PvEGS] GamePhase replicated %d -> %d"),
        static_cast<int32>(OldPhase),
        static_cast<int32>(GamePhase));
}
