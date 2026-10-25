#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "TimerManager.h"
#include "ASCENDUNT/GameModes/PVE/AscPvEGameTypes.h"
#include "ASCENDUNT/HeroSelection/AscHeroSelectionAuthorityInterface.h"
#include "AscPvEGameMode.generated.h"

class AAscPvEGameState;
class AAscenduntPlayerController;
class AAscMissionDirector;
class AHeroCharacter;
class UAscHeroSelectionComponent;

/**
 * Lightweight authoritative GameMode for the 3-player PvE mission.
 *
 * Current authoritative PvE responsibilities:
 * - admit up to three players and run initial hero selection
 * - spawn the confirmed squad and enter Playing
 * - own full-death -> spectator transition (no PvE respawn)
 * - evaluate the squad-wide Alive count and enter MissionFailed on a wipe
 *
 * Detailed Downed/revive behavior remains owned by the hero-side PvE component.
 * Mission/objective/encounter sequencing belongs to separate systems.
 */
UCLASS()
class ASCENDUNT_API AAscPvEGameMode : public AGameMode, public IAscHeroSelectionAuthorityInterface
{
    GENERATED_BODY()

public:
    AAscPvEGameMode();

    virtual void BeginPlay() override;

    virtual void InitGame(
        const FString& MapName,
        const FString& Options,
        FString& ErrorMessage) override;

    virtual void InitGameState() override;

    virtual void PreLogin(
        const FString& Options,
        const FString& Address,
        const FUniqueNetIdRepl& UniqueId,
        FString& ErrorMessage) override;

    virtual APlayerController* Login(
        UPlayer* NewPlayer,
        ENetRole InRemoteRole,
        const FString& Portal,
        const FString& Options,
        const FUniqueNetIdRepl& UniqueId,
        FString& ErrorMessage) override;

    virtual void PostLogin(APlayerController* NewPlayer) override;

    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
    virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

    /** IAscHeroSelectionAuthorityInterface */
    virtual bool HandleHeroConfirmRequest(
        AAscenduntPlayerController* PlayerController,
        int32 HeroId,
        EAscHeroSelectionContext Context) override;

    /** Temporary phase setter kept for BP TriggerBox testing. */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|PvE")
    void SetGamePhase(EAscPvEGamePhase NewPhase);

    UFUNCTION(BlueprintPure, Category = "Ascendunt|PvE")
    AAscPvEGameState* GetPvEGameState() const;

    /** Called by the DownedComponent whenever a hero enters/leaves an incapacitated life state. */
    void NotifyHeroLifeStateChanged(AHeroCharacter* Hero);

    /** Final-death owner for PvE. No respawn is scheduled; the player remains spectating. */
    void HandleHeroFullDeath(AHeroCharacter* DeadHero);

protected:
    /** PvE is currently fixed to a three-player squad. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|PvE", meta = (ClampMin = "1"))
    int32 RequiredPlayers = 3;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|PvE", meta = (ClampMin = "1"))
    int32 MaxPlayers = 3;

    /** All PvE players are allies and use TeamId 1. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|PvE")
    uint8 PvETeamId = 1;

    /** Prevent simultaneous initial squad spawns from selecting the same/overlapping start. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|PvE|Spawn", meta = (ClampMin = "0.0"))
    float PlayerStartOccupancyRadius = 160.0f;

    /**
     * Shared validation/confirm implementation already used by PvP/training.
     * Assign the same HeroRosterData used by the hero-selection UI in BP_PvEGameMode.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ascendunt|Hero Selection")
    TObjectPtr<UAscHeroSelectionComponent> HeroSelectionComponent;

    /** Keep policy configurable without adding PvE-specific selection code. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Hero Selection")
    FAscHeroSelectionRules HeroSelectionRules;

private:
    UFUNCTION()
    void HandleMissionCompleted(AAscMissionDirector* MissionDirector);

    UFUNCTION()
    void HandleMissionFailed(AAscMissionDirector* MissionDirector);

    bool AreAllRequiredPlayersConfirmed() const;
    bool IsPlayerStartAvailable(const AActor* PlayerStart, const AController* Player) const;
    bool SpawnConfirmedHero(AAscenduntPlayerController* PlayerController);
    bool SpawnAllConfirmedHeroes();
    void TryStartPlaying();
    void BindMissionDirector();
    void FinishMission(bool bMissionSuccess);
    void BeginMissionExitTransition();
    void EvaluateSquadWipe();
    int32 CountAliveHeroes() const;

    /** Time reserved for Mission Success/Fail title presentation before the fade/cleanup hook starts. */
    UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|PvE|Mission End", meta = (ClampMin = "0.0"))
    float MissionResultHoldDuration = 3.0f;

    /** Delay after the local fade/cleanup hook before travelling to the main menu. */
    UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|PvE|Mission End", meta = (ClampMin = "0.0"))
    float MissionExitTransitionDuration = 1.0f;

    TWeakObjectPtr<AAscMissionDirector> BoundMissionDirector;
    FTimerHandle MissionExitTimerHandle;
    bool bMissionEndStarted = false;
    bool bPendingMissionSuccess = false;
};
