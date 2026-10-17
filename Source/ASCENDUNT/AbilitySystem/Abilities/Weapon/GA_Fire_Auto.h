// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/AbilitySystem/Abilities/Weapon/GA_BaseFire.h"
#include "GA_Fire_Auto.generated.h"

class UAbilityTask_WaitDelay;
/**
 * UGA_Fire_Auto
 * - WhileInputActive
 * - 눌러있는 동안 ROF 간격으로 반복 발사, Release 시 즉시 종료
 */
UCLASS()
class ASCENDUNT_API UGA_Fire_Auto : public UGA_BaseFire
{
	GENERATED_BODY()
	
public:
	UGA_Fire_Auto();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	UFUNCTION()
	void HandleTickFire();

	void ScheduleNextTick();

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> TickTask;

	bool bStopRequested = false;
	int32 ShotIndex = 0;
};
