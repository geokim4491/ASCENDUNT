// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "NativeGameplayTags.h"

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AscenduntAbilitySystemComponent.generated.h"

class UAscenduntGameplayAbility;
class UTexture2D;
enum class EAscenduntAbilityActivationGroup : uint8;

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Gameplay_AbilityInputBlocked);

USTRUCT(BlueprintType)
struct FAscAbilityUIState
{
	GENERATED_BODY()

	/** Icon comes from UAscAbilityData::AbilityIcon. */
	UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Ability UI")
	TObjectPtr<UTexture2D> AbilityIcon = nullptr;

	/** Charge-based skills show this number over the icon. */
	UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Ability UI")
	bool bUsesStock = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Ability UI")
	int32 CurrentStock = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Ability UI")
	int32 MaxStock = 0;

	/** True while a stock is recharging or a normal GAS cooldown is active. */
	UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Ability UI")
	bool bShowCooldownOverlay = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Ability UI")
	float TimeRemaining = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Ability UI")
	float Duration = 0.0f;

	/** 1 immediately after use, decreasing to 0 as the yellow overlay is removed. */
	UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Ability UI")
	float OverlayPercent = 0.0f;
};

/**
 * 
 */
UCLASS()
class ASCENDUNT_API UAscenduntAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
public:
	UAscenduntAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	//~UActorComponent interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of UActorComponent interface

	void AbilityInputTagPressed(const FGameplayTag& InputTag);
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

	virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec) override;
	virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec) override;
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;
	void RemovePredictedWeaponAmmoCost(FActiveGameplayEffectHandle Handle, const class AAscenduntWeapon* Weapon);
	UFUNCTION(Client, Reliable) void Client_WeaponReloadFinished(class AAscenduntWeapon* Weapon, int16 PredictionKey);
	void ProcessAbilityInput(float DeltaTime, bool bGamePaused);
	void ClearAbilityInput();

	/** Re-runs ActivationPolicy::OnSpawn abilities after a preserved AbilitySet receives a new AvatarActor. */
	void TryActivateAbilitiesOnSpawn();

	/** UI helper: returns the replicated/predicted GAS cooldown for an ability identity tag. */
	UFUNCTION(BlueprintPure, Category = "Ascendunt|Ability|Cooldown")
	bool GetAbilityCooldownByAbilityTag(FGameplayTag AbilityTag, float& TimeRemaining, float& CooldownDuration) const;

	/** UI helper: same query using the InputTag assigned by AbilitySet. */
	UFUNCTION(BlueprintPure, Category = "Ascendunt|Ability|Cooldown")
	bool GetAbilityCooldownByInputTag(FGameplayTag InputTag, float& TimeRemaining, float& CooldownDuration) const;

	/** Unified HUD query: stock recharge for charge skills, GAS cooldown for normal skills. */
	UFUNCTION(BlueprintPure, Category = "Ascendunt|Ability|UI")
	bool GetAbilityUIStateByInputTag(FGameplayTag InputTag, FAscAbilityUIState& OutState) const;


	bool IsActivationGroupBlocked(EAscenduntAbilityActivationGroup Group) const;

	void AddAbilityToActivationGroup(EAscenduntAbilityActivationGroup Group, UAscenduntGameplayAbility* Ability);
	void RemoveAbilityFromActivationGroup(EAscenduntAbilityActivationGroup Group, UAscenduntGameplayAbility* Ability);

	void CancelActivationGroupAbilities(EAscenduntAbilityActivationGroup Group, UAscenduntGameplayAbility* IgnoreAbility = nullptr);

	/**
	 * Server-confirmed hit/heal feedback for this ASC's owning player only.
	 * FeedbackTarget remains the damaged/healed actor so cues can attach there.
	 * Cosmetic and intentionally unreliable; callers must run on the server.
	 */
	UFUNCTION(Client, Unreliable)
	void Client_PlayCombatFeedback(AActor* FeedbackTarget, FGameplayTag CueTag, FGameplayCueParameters Params);


protected:
	// Handles to abilities that had their input pressed this frame.
	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;

	// Handles to abilities that had their input released this frame.
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;

	// Handles to abilities that have their input held.
	TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;

	TArray<TArray<TWeakObjectPtr<UAscenduntGameplayAbility>>> ActivationGroupAbilities;
};
