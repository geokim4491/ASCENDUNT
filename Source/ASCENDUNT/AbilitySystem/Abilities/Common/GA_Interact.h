#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/AbilitySystem/Abilities/AscenduntGameplayAbility.h"
#include "ASCENDUNT/Interaction/AscInteractionTypes.h"
#include "GA_Interact.generated.h"

class UAbilityTask_WaitDelay;
class UAscInteractionComponent;
class AHeroCharacter;

/**
 * Common V-key interaction ability.
 *
 * Target selection is deterministic/local-query based on UAscInteractionComponent.
 * Gameplay callbacks on IAscInteractable are authority-only; the owning client only
 * drives prompt/progress presentation and prediction-friendly movement locking.
 */
UCLASS()
class ASCENDUNT_API UGA_Interact : public UAscenduntGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_Interact(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
    virtual bool CanActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags,
        const FGameplayTagContainer* TargetTags,
        FGameplayTagContainer* OptionalRelevantTags) const override;

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    virtual void InputReleased(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo) override;

    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled) override;

private:
    UFUNCTION()
    void HandleInteractionTick();

    bool ResolveInteractionTarget(
        const FGameplayAbilityActorInfo* ActorInfo,
        AActor*& OutTarget,
        FAscInteractionOption& OutOption) const;

    bool ValidateActiveTarget(FAscInteractionOption* OutCurrentOption = nullptr) const;
    void CompleteInteraction();
    void ScheduleInteractionTick();
    void SetMovementLocked(bool bLocked);
    double GetCurrentWorldTimeSeconds() const;

private:
    UPROPERTY()
    TObjectPtr<AActor> ActiveTarget;

    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitDelay> InteractionTickTask;

    FAscInteractionOption ActiveOption;
    double InteractionStartTime = 0.0;
    bool bServerInteractionBegun = false;
    bool bCompleted = false;
    bool bMovementLockApplied = false;

    UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|Interaction", meta = (ClampMin = "0.01"))
    float ValidationInterval = 0.05f;
};
