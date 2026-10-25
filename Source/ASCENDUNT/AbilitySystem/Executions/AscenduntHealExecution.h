// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "AscenduntHealExecution.generated.h"

/**
 * 
 *  UAscenduntHealExecution
 * - Source: SetByCaller(Data.Weapon.BaseHeal) 또는 SetByCaller(Data.HealOverride)
 * - Target: HealthSet.Healing(meta)로 바로 적립
 * - HealReceivedScalar와 Data.DamageCoef를 반영
 * 
 */
UCLASS()
class ASCENDUNT_API UAscenduntHealExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()
	
public:
	UAscenduntHealExecution();

	virtual void Execute_Implementation(
		const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
