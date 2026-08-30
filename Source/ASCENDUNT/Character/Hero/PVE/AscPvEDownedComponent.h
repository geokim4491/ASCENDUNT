#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "ASCENDUNT/Interaction/AscInteractionTypes.h"
#include "AscPvEDownedComponent.generated.h"

class AHeroCharacter;
class UAscPvEDownedConfig;

DECLARE_MULTICAST_DELEGATE_OneParam(FAscPvEDownedStateChanged, bool /*bIsDowned*/);
DECLARE_MULTICAST_DELEGATE(FAscPvERevivePresentationChanged);

/**
 * PvE life state between DeathStarted and DeathFinished.
 *
 * Responsibilities:
 * - enter Downed instead of immediately finishing death
 * - expose replicated Downed / bleedout / being-revived state
 * - crawl movement and Downed presentation
 * - block normal abilities/healing/damage while Downed
 * - own the revive interaction policy and authoritative revive result
 * - transition to full death when bleedout expires
 */
UCLASS(ClassGroup=(Ascendunt), meta=(BlueprintSpawnableComponent))
class ASCENDUNT_API UAscPvEDownedComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAscPvEDownedComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    /** Injected once by AHeroCharacter from the project-wide DA_PvEDownedConfig asset. */
    void SetConfig(UAscPvEDownedConfig* InConfig) { Config = InConfig; }

    const UAscPvEDownedConfig* GetConfig() const { return Config.Get(); }

    /** Server-only entry point. DeathStarted remains active while the hero is Downed. */
    bool EnterDowned();

    UFUNCTION(BlueprintPure, Category = "Ascendunt|PvE|Downed")
    bool IsDowned() const { return bIsDowned; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|PvE|Downed")
    bool IsBeingRevived() const { return ActiveReviver != nullptr; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|PvE|Downed")
    AHeroCharacter* GetActiveReviver() const { return ActiveReviver.Get(); }

    /** True on the living hero currently holding a revive interaction on another hero. */
    UFUNCTION(BlueprintPure, Category = "Ascendunt|PvE|Downed")
    bool IsRevivingOther() const { return ReviveTarget != nullptr; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|PvE|Downed")
    AHeroCharacter* GetReviveTarget() const { return ReviveTarget.Get(); }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|PvE|Downed")
    float GetBleedoutEndServerTime() const { return BleedoutEndServerTime; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|PvE|Downed")
    float GetBleedoutRemainingTime() const;

    /** Used by AHeroCharacter's native IAscInteractable implementation. */
    bool CanBeRevivedBy(const AHeroCharacter* Interactor) const;
    FAscInteractionOption GetReviveInteractionOption() const;

    /** Server-authoritative callbacks delegated from the common interaction interface. */
    bool BeginRevive(AHeroCharacter* Interactor);
    void CancelRevive(AHeroCharacter* Interactor);
    bool CompleteRevive(AHeroCharacter* Interactor);

    /** Local presentation notifications. Runtime state remains authoritative/replicated above. */
    FAscPvEDownedStateChanged OnDownedStateChanged;
    FAscPvERevivePresentationChanged OnRevivePresentationChanged;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_IsDowned();

    UFUNCTION()
    void OnRep_ActiveReviver();

    UFUNCTION()
    void OnRep_ReviveTarget();

    void ApplyDownedStateLocally(bool bEnable);
    void ApplyBeingRevivedStateLocally();

    void HandleBleedoutExpired();
    void EnterFullDeath();

    bool ApplyReviveHealing(AHeroCharacter* Interactor);
    void ApplyReviveGraceInvincibility();
    void ClearReviveGraceInvincibility();
    void HandleReviveGraceExpired();
    void SetActiveReviver(AHeroCharacter* NewReviver);
    void SetReviveTarget(AHeroCharacter* NewTarget);

protected:
    /** Shared immutable tuning asset. Not replicated; every Hero loads the same fixed asset path. */
    UPROPERTY(Transient)
    TObjectPtr<UAscPvEDownedConfig> Config = nullptr;

    UPROPERTY(ReplicatedUsing = OnRep_IsDowned)
    bool bIsDowned = false;

    /** Absolute server world time. UI can derive remaining time without per-frame RPCs. */
    UPROPERTY(Replicated)
    float BleedoutEndServerTime = 0.0f;

    /** Non-null while exactly one teammate owns the revive interaction on this target. */
    UPROPERTY(ReplicatedUsing = OnRep_ActiveReviver)
    TObjectPtr<AHeroCharacter> ActiveReviver = nullptr;

    /** Replicated on the reviver as presentation/gameplay state for external consumers. */
    UPROPERTY(ReplicatedUsing = OnRep_ReviveTarget)
    TObjectPtr<AHeroCharacter> ReviveTarget = nullptr;

private:
    FTimerHandle BleedoutTimerHandle;
    FTimerHandle ReviveGraceTimerHandle;

    bool bAbilityInputBlockApplied = false;
    bool bInvincibleTagApplied = false;
    bool bReviveGraceInvincibleApplied = false;
};
