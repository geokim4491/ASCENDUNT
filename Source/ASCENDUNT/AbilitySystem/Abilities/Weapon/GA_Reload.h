// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/AbilitySystem/Abilities/AscenduntGameplayAbility.h"
#include "GA_Reload.generated.h"

class AAscenduntWeapon;
class UAbilityTask_PlayMontageAndWait;
class UAT_PlayWeaponMontageAndWait;

/**
 * 
 */
UCLASS()
class ASCENDUNT_API UGA_Reload : public UAscenduntGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Reload();

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	    const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	    const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	UPROPERTY()
	TObjectPtr<AAscenduntWeapon> CachedWeapon;

	int32 CachedMagSize = 0;

	UFUNCTION()
	void OnReloadDelayFinished();

	UPROPERTY()
	TObjectPtr<UAT_PlayWeaponMontageAndWait> WeaponMontageTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> HeroMontageTask;
};
