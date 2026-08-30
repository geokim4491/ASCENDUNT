#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "ASCENDUNT/GameModes/PVE/AscPvEGameTypes.h"
#include "AscPvEGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnAscPvEGamePhaseChanged,
    EAscPvEGamePhase, OldPhase,
    EAscPvEGamePhase, NewPhase);

/**
 * Lightweight replicated state for ASCENDUNT PvE.
 *
 * Keep only client-visible PvE match state here. Mission/objective/encounter
 * logic belongs to separate systems added later.
 */
UCLASS()
class ASCENDUNT_API AAscPvEGameState : public AGameState
{
    GENERATED_BODY()

public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category = "Ascendunt|PvE")
    EAscPvEGamePhase GetGamePhase() const { return GamePhase; }

    /** Server-only state change entry point. */
    void SetGamePhase(EAscPvEGamePhase NewPhase);

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|PvE")
    FOnAscPvEGamePhaseChanged OnGamePhaseChanged;

protected:
    UFUNCTION()
    void OnRep_GamePhase(EAscPvEGamePhase OldPhase);

    UPROPERTY(ReplicatedUsing = OnRep_GamePhase, BlueprintReadOnly, Category = "Ascendunt|PvE")
    EAscPvEGamePhase GamePhase = EAscPvEGamePhase::HeroSelection;
};
