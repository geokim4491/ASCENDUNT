// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ASCENDUNT/Match/AscMatchTypes.h"
#include "AscenduntPlayerController.generated.h"

class UUserWidget;
class AAscenduntPlayerState;
class UAscCharacterSelectionWidget;
class UAscInGameHUDWidget;
class UAscScoreboardWidget;
class UAscHeroHUDPanelWidget;
class UHeroInputComponent;
class UAscHeroRosterData;
class UAscenduntCameraComponent;
class UAscenduntCameraMode;
class UAscTargetAwarenessComponent;

/**
 * PlayerController trimmed after removing the old Session/Lobby system.
 *
 * Responsibilities kept here:
 * - basic local root UI switching by map type
 * - player team debug command / server RPC
 * - possession-time team propagation to the pawn
 *
 */
UCLASS()
class ASCENDUNT_API AAscenduntPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AAscenduntPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnRep_Pawn() override;
    virtual void OnRep_PlayerState() override;
    virtual void PostSeamlessTravel() override;
    virtual void SetupInputComponent() override;
    virtual void PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel) override;

    UFUNCTION(Exec)
    void SetTeam(uint8 NewTeam);

    UFUNCTION(Server, Reliable)
    void Server_SetTeam(uint8 NewTeam);

    /**
     * Confirms a hero selection on the server.
     * UI click/preview should stay local; only the confirm button should call this.
     */
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Ascendunt|Hero Selection")
    void Server_RequestConfirmHero(int32 HeroId, EAscHeroSelectionContext Context);

    /** Console/UI test helper. In training range this confirms through the shared InGameRespawnRoom context. */
    UFUNCTION(Exec)
    void ConfirmHero(int32 HeroId);

    /** Opens the reusable hero selection UI locally. Used only by validated client RPC/local UI flows. */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Hero Selection")
    void OpenHeroSelection(EAscHeroSelectionContext Context);

    /** H-key GA path. The server validates respawn-room state before opening UI. */
    UFUNCTION(Server, Reliable)
    void Server_RequestOpenRespawnRoomHeroSelection();

    /** H-key while dead. Server validates DeadWaitingRespawn before opening the UI. */
    UFUNCTION(Server, Reliable)
    void Server_RequestOpenDeadHeroSelection();

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Hero Selection")
    void CloseHeroSelection();

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Hero Selection")
    bool IsHeroSelectionOpen() const;

    /** Server asks the owning client to show the reusable hero selection UI. */
    UFUNCTION(Client, Reliable, BlueprintCallable, Category = "Ascendunt|Hero Selection")
    void Client_OpenHeroSelection(EAscHeroSelectionContext Context);

    /** Server asks the owning client to close the reusable hero selection UI. */
    UFUNCTION(Client, Reliable, BlueprintCallable, Category = "Ascendunt|Hero Selection")
    void Client_CloseHeroSelection();

    /** Server sends the result of a confirm attempt back to the owning client. */
    UFUNCTION(Client, Reliable, BlueprintCallable, Category = "Ascendunt|Hero Selection")
    void Client_NotifyHeroConfirmResult(bool bAccepted, EAscHeroSelectionRejectReason RejectReason);

    /** BP hook for creating/showing WBP_HeroSelection. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Ascendunt|Hero Selection")
    void BP_OnOpenHeroSelection(EAscHeroSelectionContext Context);

    /** BP hook for closing the selection UI or showing rejection reason. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Ascendunt|Hero Selection")
    void BP_OnHeroConfirmResult(bool bAccepted, EAscHeroSelectionRejectReason RejectReason);

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Player")
    AAscenduntPlayerState* GetAscenduntPS() const;

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Target Awareness")
    UAscTargetAwarenessComponent* GetTargetAwarenessComponent() const { return TargetAwarenessComponent; }

    /** Server/GameMode/BP can call this on an owning client to show a temporary in-game HUD text. */
    UFUNCTION(Client, Reliable, BlueprintCallable, Category = "Ascendunt|UI")
    void Client_ShowInGameMessage(const FText& Message, float DurationSeconds = 2.0f);

    /** Frequent non-lethal hit confirmation. Cosmetic only, so an occasional dropped packet is acceptable. */
    UFUNCTION(Client, Unreliable)
    void Client_ShowHitMarker(bool bHeadshot);

    /**
     * Killing hit confirmation. One reliable RPC starts both the body/headshot marker and kill marker
     * in the same local function, guaranteeing that the two UI layers begin together.
     */
    UFUNCTION(Client, Reliable)
    void Client_ShowHitAndKillMarker(bool bHeadshot);

    UFUNCTION(BlueprintPure, Category = "Ascendunt|UI")
    UAscInGameHUDWidget* GetActiveInGameHUDWidget() const { return ActiveInGameHUDWidget; }

    /** Local-only hold scoreboard. Does not change input mode or send an RPC. */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|UI|Scoreboard")
    void ShowScoreboard();

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|UI|Scoreboard")
    void HideScoreboard();

    UFUNCTION(BlueprintPure, Category = "Ascendunt|UI|Scoreboard")
    UAscScoreboardWidget* GetActiveScoreboardWidget() const { return ActiveScoreboardWidget; }

    const UAscHeroRosterData* GetHeroRosterData() const { return HeroRosterData; }

    /** May be called by the ESC menu during any match phase, including Playing. */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Match")
    void LeaveCurrentMatchToMainMenu();

    UFUNCTION(Client, Reliable)
    void Client_BeginPostMatch(float TimeDilationScale);

    UFUNCTION(Client, Reliable)
    void Client_EndPostMatchSlowMotion();

    UFUNCTION(Client, Reliable)
    void Client_ReturnToMainMenu(const FString& TargetMainMenuMapPath);

    /**
     * PvE-only final transition. Lets the HUD run its local fade/cleanup hook, then returns to main menu.
     * Successful missions leave a one-shot GameInstance flag so the main menu can present the clear video.
     */
    UFUNCTION(Client, Reliable)
    void Client_BeginPvEMissionExitTransition(bool bMissionSuccess, float TravelDelaySeconds);

    /** Enters local death spectating. Space cycles death-position/allied live pawns. */
    UFUNCTION(Client, Reliable)
    void Client_EnterDeathSpectating(AActor* DeathViewActor, float RespawnEndServerTime);

    UFUNCTION(Client, Reliable)
    void Client_ExitDeathSpectating(APawn* NewPawn);

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Respawn")
    bool IsDeathSpectating() const { return bDeathSpectating; }

    /** Called by MatchGameMode on both the server and owning client copy. */
    void SetMatchGameplayBlocked(bool bBlocked);

    // Owning-client input mapping ownership. Prevents a late EndPlay from an old Pawn
    // removing mapping contexts already installed by the newly possessed hero.
    void RegisterActiveHeroInputComponent(UHeroInputComponent* NewComponent);
    void UnregisterActiveHeroInputComponent(UHeroInputComponent* Component);
    bool IsActiveHeroInputComponent(const UHeroInputComponent* Component) const;

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|UI")
    TSubclassOf<UUserWidget> MainMenuWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|UI")
    TSubclassOf<UAscInGameHUDWidget> MatchWidgetClass;

    /** Optional WBP derived from UAscScoreboardWidget. Created only on the owning client. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|UI|Scoreboard")
    TSubclassOf<UAscScoreboardWidget> ScoreboardWidgetClass;

    /** Shared roster used by the common HUD for portraits and optional hero panels. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|UI")
    TObjectPtr<const UAscHeroRosterData> HeroRosterData;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Match")
    FString MainMenuMapPath = TEXT("/Game/Maps/L_MainMenu");

    /** Optional generic hero selection widget used by training/match/respawn-room flows. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|UI")
    TSubclassOf<UAscCharacterSelectionWidget> HeroSelectionWidgetClass;

    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> ActiveRootWidget;

    UPROPERTY(Transient)
    TObjectPtr<UAscInGameHUDWidget> ActiveInGameHUDWidget;

    UPROPERTY(Transient)
    TObjectPtr<UAscScoreboardWidget> ActiveScoreboardWidget;

    UPROPERTY(Transient)
    TObjectPtr<UAscCharacterSelectionWidget> ActiveHeroSelectionWidget;

    UPROPERTY(Transient)
    TObjectPtr<UHeroInputComponent> ActiveHeroInputComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ascendunt|Target Awareness", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UAscTargetAwarenessComponent> TargetAwarenessComponent;

protected:
    void SetupUIForCurrentMap();
    void ShowHeroSelectionUI(EAscHeroSelectionContext Context);

    void HideHeroSelectionUI();
    void ClearCurrentWidget();
    void ShowMainMenuUI();
    void ShowMatchUI();
    void EnsureScoreboardWidget();
    void ClearScoreboardWidget();
    void HandleScoreboardPressed();
    void HandleScoreboardReleased();
    void RefreshInGameHUDForPawn(APawn* NewPawn);
    TSubclassOf<UAscHeroHUDPanelWidget> ResolveHeroHUDPanelClass(const APawn* Pawn) const;
    void ApplyInputMode_UIOnly();
    void ApplyInputMode_GameOnly();
    void StopControlledPawnMovementAndClearAbilityInput();
    void SetHeroSelectionInputBlocked(bool bBlocked);
    void RefreshGameplayInputBlockedState();
    void ReturnToMainMenuInternal(const FString& TargetMapPath);
    void FinishPvEMissionReturnToMainMenu();
    void HandleSpectateNextPressed();
    void HandleDeadHeroSelectionPressed();
    void ValidateDeathSpectateTarget();
    void CycleDeathSpectateTarget();
    void SetDeathSpectateViewTarget(AActor* NewTarget);
    void BindDeathSpectateCamera(AActor* Target);
    void ReleaseDeathSpectateCameraBinding();
    TSubclassOf<UAscenduntCameraMode> DetermineDeathSpectateCameraMode() const;
    void HandleDeathSpectateMouseYaw(float Value);
    void HandleDeathSpectateMousePitch(float Value);
    void GatherAliveAlliedSpectateTargets(TArray<AActor*>& OutTargets) const;
    bool IsValidAliveAlliedSpectateTarget(const AActor* Target) const;
    bool ShouldCloseHeroSelectionOnAcceptedConfirm() const;
    bool CanCloseHeroSelectionFromInput() const;
    FString GetCurrentMapName() const;
    bool IsMainMenuMap() const;
    bool IsMatchMap() const;

    UPROPERTY(Transient)
    TObjectPtr<AActor> DeathViewTarget;

    UPROPERTY(Transient)
    TObjectPtr<AActor> CurrentDeathSpectateTarget;

    /** Forced camera mode for both the ragdoll corpse and living allied spectate targets. */
    UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|Respawn|Camera")
    TSubclassOf<UAscenduntCameraMode> DeathSpectatorCameraModeClass;

    UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|Respawn|Camera")
    float DeathSpectateMouseYawScale = 1.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|Respawn|Camera")
    float DeathSpectateMousePitchScale = -1.0f;

    FTimerHandle DeathSpectateValidationTimer;

    TWeakObjectPtr<UAscenduntCameraComponent> DeathSpectateBoundCamera;

    bool bDeathSpectating = false;
    bool bHeroSelectionInputBlocked = false;
    bool bMatchGameplayBlocked = false;
    bool bMoveInputCurrentlyBlocked = false;
    bool bLookInputCurrentlyBlocked = false;
    bool bAbilityInputBlockTagApplied = false;
    FTimerHandle PvEMissionReturnTimerHandle;
};
