#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "ASCENDUNT/Match/AscMatchTypes.h"
#include "ASCENDUNT/Interaction/AscInteractionTypes.h"
#include "AscInGameHUDWidget.generated.h"
class UAscenduntAbilitySystemComponent;
struct FOnAttributeChangeData;

class AAscenduntPlayerController;
class AAscenduntWeapon;
class AMatchGameState;
class APawn;
class UAscMapIntroWidget;
class UAscMissionObjectiveWidget;
class UAscPlayerHealthPanelWidget;
class UAscHeroHUDPanelWidget;
class UAscInteractionComponent;
class UAscPvEDownedComponent;
class UImage;
class UOverlay;
class UTextBlock;
class UProgressBar;
class UTexture2D;
class UWeaponEquipmentComponent;
class UAbilitySystemComponent;
struct FOnAttributeChangeData;


USTRUCT(BlueprintType)
struct ASCENDUNT_API FAscHitMarkerAnimationSettings
{
    GENERATED_BODY()

    /** Time used to fade from transparent to fully visible. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Marker", meta = (ClampMin = "0.0"))
    float FadeInDuration = 0.025f;

    /** Fully-visible hold time before fading out. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Marker", meta = (ClampMin = "0.0"))
    float HoldDuration = 0.035f;

    /** Time used to fade from fully visible to transparent. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Marker", meta = (ClampMin = "0.0"))
    float FadeOutDuration = 0.14f;

    /** Scale animation duration. Scale continues to EndScale after FadeIn if this is longer. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Marker", meta = (ClampMin = "0.0"))
    float ScaleUpDuration = 0.08f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Marker", meta = (ClampMin = "0.01"))
    float StartScale = 0.72f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Marker", meta = (ClampMin = "0.01"))
    float EndScale = 1.0f;
};


struct FAscHitMarkerRuntimeState
{
    float Elapsed = 0.0f;
    bool bPlaying = false;
};

/**
 * Shared in-game HUD used by both training range and match maps.
 *
 * Common HUD data is updated from pawn/weapon delegates; the child health panel owns health binding.
 * Match-only data is updated from AMatchGameState delegates, so the widget also
 * works in TrainingGameMode where no MatchGameState is present.
 */
UCLASS()
class ASCENDUNT_API UAscInGameHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UAscInGameHUDWidget(const FObjectInitializer& ObjectInitializer);

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|InGame HUD")
    void InitializeInGameHUD(AAscenduntPlayerController* InOwningAscPC);

    /** Called by the owning PlayerController when its replicated Pawn changes. */
    virtual void NotifyOwningPawnChanged(APawn* NewPawn);

    /** Replaces only the optional hero-specific section; the common HUD remains alive. */
    void SetHeroHUDPanelClass(TSubclassOf<UAscHeroHUDPanelWidget> NewPanelClass);

    /** Shows a temporary center text. Duration <= 0 keeps it visible until ClearCenterMessage(). */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|InGame HUD")
    void ShowCenterMessage(const FText& Message, float DurationSeconds = 2.0f);

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|InGame HUD")
    void ClearCenterMessage();

    /** Permanent small phase/status text controlled by BP or PlayerController RPC. Empty text hides it. */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|InGame HUD")
    void SetPhaseMessageText(const FText& Message);

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|InGame HUD")
    void SetPortraitTexture(UTexture2D* InPortrait);

    /**
     * Plays server-confirmed hit feedback for the owning player.
     * Headshot replaces the normal hit image, while Kill is an independent layer and can play simultaneously.
     */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|InGame HUD|Hit Marker")
    void ShowHitMarker(bool bHeadshot, bool bKilled);

    /**
     * Called after the Mission Success/Fail title has had its hold time and before ClientTravel.
     * C++ deliberately does not decide which roots are hidden here; WBP owns the fade/cleanup composition.
     */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|PvE|Mission End")
    void BeginPvEMissionExitTransition(bool bMissionSuccess, float TravelDelaySeconds);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    /**
     * Implement in WBP_InGameHUD for planned cleanup such as collapsing CombatRoot and fading to black.
     * MissionTitleRoot can be left visible independently in the WBP hierarchy.
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Ascendunt|PvE|Mission End")
    void BP_OnPvEMissionExitTransition(bool bMissionSuccess, float TravelDelaySeconds);


    UFUNCTION()
    void HandleAmmoChanged(int CurrentAmmo, int MaxAmmo);

    UFUNCTION()
    void HandleFocusedInteractionChanged(AActor* TargetActor, FText PromptText, EAscInteractionInputMode InputMode, float HoldDuration);

    UFUNCTION()
    void HandleInteractionProgressChanged(float Progress, bool bIsInteracting);

    void HandleObservedDownedStateChanged(bool bIsDowned);
    void RefreshDownedHUD();

    void HandleMatchGameStateChanged();
    void HandleCurrentWeaponChanged(AAscenduntWeapon* NewWeapon, AAscenduntWeapon* PrevWeapon);

    void BindMatchGameState();
    void UnbindMatchGameState();
    void BindObservedPawn(APawn* NewPawn);
    void UnbindObservedPawn();
    void BindObservedWeapon(AAscenduntWeapon* NewWeapon);
    void UnbindObservedWeapon();
    void BindObservedInteraction(UAscInteractionComponent* NewInteraction);
    void UnbindObservedInteraction();
    void BindObservedDowned(UAscPvEDownedComponent* NewDowned);
    void UnbindObservedDowned();

    /** Performs one initial synchronization. Runtime updates are delegate-driven. */
    void RefreshAll();
    void RefreshAmmo();
    void RefreshPortrait();
    void RefreshMatchStateText();

    void BindUltimateAttributes();
    void UnbindUltimateAttributes();
    void RefreshUltimateUI();
    void HandleUltimateChargeChanged(const FOnAttributeChangeData& ChangeData);
    void HandleMaxUltimateChargeChanged(const FOnAttributeChangeData& ChangeData);

    void ShowMatchResultUI(EAscMatchResult MatchResult);
    void ClearHeroHUDPanel();

    void StartMarkerAnimation(UImage* Image, FAscHitMarkerRuntimeState& State);
    void UpdateMarkerAnimation(UImage* Image, FAscHitMarkerRuntimeState& State, const FAscHitMarkerAnimationSettings& Settings, float DeltaTime);
    void ResetHitMarkerPresentation();

protected:
    // 일반 인게임 HUD 전체를 감싸는 Root
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly,
        Category = "Ascendunt|InGame HUD")
    TObjectPtr<UWidget> GameplayHUDRoot;

    /** Optional child WBP name: MapIntroWidget. Parent class must be UAscMapIntroWidget. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|InGame HUD|Map Intro")
    TObjectPtr<UAscMapIntroWidget> MapIntroWidget;

    /** Optional child WBP name: MissionObjectiveWidget. Parent class must be UAscMissionObjectiveWidget. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|InGame HUD|Mission")
    TObjectPtr<UAscMissionObjectiveWidget> MissionObjectiveWidget;

    /** Optional WBP name: Txt_Ammo */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|InGame HUD")
    TObjectPtr<UTextBlock> Txt_Ammo;

    /** Optional WBP name: Txt_PhaseCountdown. Used for PreRoundWaiting countdown. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|InGame HUD")
    TObjectPtr<UTextBlock> Txt_PhaseCountdown;

    /** Optional WBP name: Txt_PhaseMessage. Free text area controlled by BP/RPC. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|InGame HUD")
    TObjectPtr<UTextBlock> Txt_PhaseMessage;

    /** Optional WBP name: Txt_CenterMessage. Used for temporary announcements. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|InGame HUD")
    TObjectPtr<UTextBlock> Txt_CenterMessage;

    /** Optional WBP name: InteractionRoot. Contains the common interaction prompt/progress UI. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|InGame HUD|Interaction")
    TObjectPtr<UWidget> InteractionRoot;

    /** Optional WBP name: Txt_InteractionPrompt. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|InGame HUD|Interaction")
    TObjectPtr<UTextBlock> Txt_InteractionPrompt;

    /** Optional WBP name: Progress_Interaction. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|InGame HUD|Interaction")
    TObjectPtr<UProgressBar> Progress_Interaction;

    /** Optional WBP name: Txt_InteractionProgress. Displays the hold percentage. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|InGame HUD|Interaction")
    TObjectPtr<UTextBlock> Txt_InteractionProgress;

    /** Optional WBP name: DownedRoot. Static icon/text may live inside; C++ only controls visibility and timer. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|InGame HUD|Downed")
    TObjectPtr<UWidget> DownedRoot;

    /** Optional WBP name: Txt_DownedRemaining. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|InGame HUD|Downed")
    TObjectPtr<UTextBlock> Txt_DownedRemaining;

    /** Optional WBP name: PlayerHealthPanel. This child owns all local health binding and presentation. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|InGame HUD")
    TObjectPtr<UAscPlayerHealthPanelWidget> PlayerHealthPanel;

    /** Optional WBP name: Img_Portrait */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|InGame HUD")
    TObjectPtr<UImage> Img_Portrait;

    /** Normal body-hit marker. Add an Image named Img_HitMarker to WBP_InGameHUD and center it. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|InGame HUD|Hit Marker")
    TObjectPtr<UImage> Img_HitMarker;

    /** Headshot marker. It occupies the same center slot as Img_HitMarker. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|InGame HUD|Hit Marker")
    TObjectPtr<UImage> Img_HeadshotMarker;

    /** Kill marker. This is independent from hit/headshot so both can be visible at the same time. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|InGame HUD|Hit Marker")
    TObjectPtr<UImage> Img_KillMarker;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|InGame HUD|Hit Marker")
    FAscHitMarkerAnimationSettings HitMarkerAnimation;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|InGame HUD|Hit Marker")
    FAscHitMarkerAnimationSettings HeadshotMarkerAnimation;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|InGame HUD|Hit Marker")
    FAscHitMarkerAnimationSettings KillMarkerAnimation;

    /** Common ultimate charge bar. Visible until charge reaches MaxUltimateCharge. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|InGame HUD|Ultimate")
    TObjectPtr<UProgressBar> Progress_UltimateCharge;

    /** Common ultimate charge number (0-100 percent). Hidden when ultimate is ready. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|InGame HUD|Ultimate")
    TObjectPtr<UTextBlock> Txt_UltimateCharge;

    /** Ready-state image shown instead of the bar/number at full charge. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|InGame HUD|Ultimate")
    TObjectPtr<UImage> Img_UltimateReady;

    /** Optional empty panel container named Overlay_HeroHUDPanel in the common HUD WBP. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|InGame HUD|Hero")
    TObjectPtr<UOverlay> Overlay_HeroHUDPanel;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|InGame HUD|Interaction")
    FText InteractionTapFormat;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|InGame HUD|Interaction")
    FText InteractionHoldFormat;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|InGame HUD|Match")
    FText PreRoundCountdownFormat;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|InGame HUD|Match")
    FText PlayingStartMessage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|InGame HUD|Match", meta = (ClampMin = "0"))
    float PlayingStartMessageDuration = 2.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|InGame HUD|Match")
    bool bShowPlayingStartMessage = true;

private:
    UPROPERTY(Transient)
    TObjectPtr<AAscenduntPlayerController> OwningAscPC;

    UPROPERTY(Transient)
    TObjectPtr<UAscHeroHUDPanelWidget> ActiveHeroHUDPanel;

    UPROPERTY(Transient)
    TSubclassOf<UAscHeroHUDPanelWidget> ActiveHeroHUDPanelClass;

    TWeakObjectPtr<AMatchGameState> CachedMatchGameState;
    TWeakObjectPtr<APawn> ObservedPawn;
    TWeakObjectPtr<UWeaponEquipmentComponent> ObservedWeaponEquipment;
    TWeakObjectPtr<UAscInteractionComponent> ObservedInteraction;
    TWeakObjectPtr<UAscPvEDownedComponent> ObservedDowned;
    TWeakObjectPtr<AAscenduntWeapon> ObservedWeapon;
    TWeakObjectPtr<UAbilitySystemComponent> BoundUltimateASC;
    FDelegateHandle UltimateChargeChangedHandle;
    FDelegateHandle MaxUltimateChargeChangedHandle;

    EAscMatchPhase LastObservedPhase = EAscMatchPhase::WaitingForPlayers;
    FTimerHandle CenterMessageTimerHandle;
    FTimerHandle DownedHUDRefreshTimerHandle;

    FAscHitMarkerRuntimeState HitMarkerRuntime;
    FAscHitMarkerRuntimeState KillMarkerRuntime;
    TWeakObjectPtr<UImage> ActiveHitMarkerImage;
    bool bActiveHitMarkerIsHeadshot = false;

    bool bMatchResultUIShown = false;
    bool bFocusedInteractionRequiresHold = false;
    void BindAmmoAttributes();
    void UnbindAmmoAttributes();
    void HandleAmmoAttributeChanged(const FOnAttributeChangeData& Data);
    TWeakObjectPtr<UAscenduntAbilitySystemComponent> BoundAmmoASC;
    FDelegateHandle AmmoAttributeHandle, MagAttributeHandle;

};
