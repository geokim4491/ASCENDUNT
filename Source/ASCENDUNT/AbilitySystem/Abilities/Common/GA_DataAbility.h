#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/AbilitySystem/Abilities/AscenduntGameplayAbility.h"
#include "GA_DataAbility.generated.h"

class AAscenduntWeapon;
class UAbilitySystemComponent;
class UAbilityTask_PlayMontageAndWait;
class UAT_PlayWeaponMontageAndWait;
class UAscAbilityData;
class UGameplayEffect;
struct FAscMontageData;

UCLASS(Abstract)
class ASCENDUNT_API UGA_DataAbility : public UAscenduntGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_DataAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Read-only access used by the shared HUD slot to obtain icon/presentation data. */
	const UAscAbilityData* GetAbilityDataForUI() const { return GetAbilityData(); }

	/** Optional charge/stock definition used by generic HUD skill slots. */
	virtual bool GetSkillStockDefinition(FGameplayTag& OutStockTag, int32& OutMaxStock, float& OutRecoverDuration) const
	{
		OutStockTag = FGameplayTag();
		OutMaxStock = 0;
		OutRecoverDuration = 0.0f;
		return false;
	}

protected:
	/** Returns the DataAsset owned by the concrete DataAbility. */
	virtual const UAscAbilityData* GetAbilityData() const { return nullptr; }

	/** GAS cooldown hooks backed by UAscAbilityData::CooldownDuration. */
	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	bool PlayAbilityMontage(const UAscAbilityData* AbilityData);

	// Plays the DataAsset montage and returns true only when this ability should wait for the
	// primary montage to finish before ending. The primary montage is HeroMontage first,
	// then WeaponMontage if HeroMontage is not set.
	bool StartAbilityMontageLifecycle(const UAscAbilityData* AbilityData);

	void FinishDataAbility(bool bWasCancelled);

	virtual void OnDataAbilityMontageFinished(bool bWasCancelled) {}

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	AAscenduntWeapon* GetCurrentWeaponFromActorInfo() const;

	bool ApplyDamageEffectToActor(AActor* TargetActor, TSubclassOf<UGameplayEffect> DamageEffectClass,
		const FHitResult& Hit, float DamageOverride, float DamageCoef) const;

	bool ApplyGameplayEffectToOwner(TSubclassOf<UGameplayEffect> EffectClass, float Level = 1.0f) const;

private:
	UFUNCTION()
	void HandleHeroMontageCompleted();

	UFUNCTION()
	void HandleHeroMontageInterrupted();

	UFUNCTION()
	void HandleWeaponMontageCompleted();

	UFUNCTION()
	void HandleWeaponMontageInterrupted();

	UFUNCTION()
	void HandleMontageFallbackCompleted();

protected:
	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> HeroMontageTask;

	UPROPERTY()
	TObjectPtr<UAT_PlayWeaponMontageAndWait> WeaponMontageTask;

private:
	FTimerHandle MontageFallbackTimer;
	bool bFinishingDataAbility = false;

	// Reused by GetCooldownTags(); mutable because GAS exposes the query as const.
	mutable FGameplayTagContainer RuntimeCooldownTags;
};
