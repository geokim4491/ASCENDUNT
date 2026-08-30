// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/AbilitySystem/Abilities/Weapon/GA_BaseFire.h"
#include "GA_Fire_Charge.generated.h"

class UCurveFloat;
class AAscenduntWeapon;

/**
* 
 * UGA_Fire_Charge
 * 
 * - ActivationPolicy: WhileInputActive
 * - 누르고 있다가 Release 순간에만 1회 발사
 * - DamageCoef는 CurveFloat로 결정 (X=HeldTimeSeconds)
 * - MaxCharge 자동발사는 없음(나중에 변경할 수도)
 * 
 */
UCLASS()
class ASCENDUNT_API UGA_Fire_Charge : public UGA_BaseFire
{
	GENERATED_BODY()
	
	
public:
	UGA_Fire_Charge();
	
protected:
	// --- runtime ---
	double ChargeStartTimeSec = 0.0;
	bool bDidFire_Server = false;

	mutable AAscenduntWeapon* CachedWeapon;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	// util
	double GetWorldTimeSecondsSafe(const FGameplayAbilityActorInfo* ActorInfo) const;
	float EvalDamageCoef(float HeldTimeSec) const;
};
