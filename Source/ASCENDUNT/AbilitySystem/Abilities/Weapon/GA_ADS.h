#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/AbilitySystem/Abilities/Weapon/GA_WeaponAbilityBase.h"
#include "GA_ADS.generated.h"

class AAscenduntWeapon;

/**
 * Local-only aim-down-sights ability granted by a weapon's WeaponAbilitySet.
 *
 * This ability owns presentation only:
 *   - scene FOV
 *   - local look sensitivity
 *   - first-person ADS pose state
 *
 * Server-authoritative changes such as movement speed or spread must not depend on this
 * LocalOnly ability. If ADS later affects gameplay, move that part to a predicted/server path.
 */
UCLASS()
class ASCENDUNT_API UGA_ADS : public UGA_WeaponAbilityBase
{
	GENERATED_BODY()

public:
	UGA_ADS();

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

	virtual void OnRemoveAbility(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilitySpec& Spec) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	void ApplyLocalADSPresentation(
		bool bEnabled,
		AAscenduntWeapon* Weapon,
		const FGameplayAbilityActorInfo* ActorInfo = nullptr);

	bool bPresentationApplied = false;
};
