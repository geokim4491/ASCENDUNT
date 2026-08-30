// Fill out your copyright notice in the Description page of Project Settings.


#include "AscenduntHealExecution.h"

#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntHealthSet.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntCharacterStatSet.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"

#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"

UAscenduntHealExecution::UAscenduntHealExecution()
{
}

void UAscenduntHealExecution::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
#if WITH_SERVER_CODE
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	// SetByCaller(Data.HealOverride)가 있으면 그 값을 우선 사용
	const float OverrideHeal = Spec.GetSetByCallerMagnitude(AscenduntGameplayTags::Data_HealOverride, false, -1.f);
	const bool bHasOverride = (OverrideHeal >= 0.f);

	float HealDone = 0.f;
	if (bHasOverride)
	{
		HealDone = FMath::Max(0.f, OverrideHeal);
	}
	else
	{
		const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
		const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

		FAggregatorEvaluateParameters EvalParams;
		EvalParams.SourceTags = SourceTags;
		EvalParams.TargetTags = TargetTags;

		float BaseHeal = Spec.GetSetByCallerMagnitude(
			AscenduntGameplayTags::Data_Weapon_BaseHeal,
			false,
			0.f
		);
		BaseHeal = FMath::Max(0.f, BaseHeal);

		UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
		float HealReceivedCoef = TargetASC->GetNumericAttribute(UAscenduntCharacterStatSet::GetHealingReceivedScalarAttribute());

		const float DamageCoef = Spec.GetSetByCallerMagnitude(AscenduntGameplayTags::Data_DamageCoef, false, 1.f);
		HealDone = BaseHeal * FMath::Max(0.f, DamageCoef) * HealReceivedCoef;
	}

	if (HealDone > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(
			FGameplayModifierEvaluatedData(UAscenduntHealthSet::GetHealingAttribute(), EGameplayModOp::Additive, HealDone)
		);
	}
#endif
}
