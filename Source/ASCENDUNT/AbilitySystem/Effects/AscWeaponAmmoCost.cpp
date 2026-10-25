#include "AscWeaponAmmoCost.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntWeaponSet.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"

UAscWeaponAmmoCostMagnitude::UAscWeaponAmmoCostMagnitude()
    : AmmoCostCapture(UAscenduntWeaponSet::GetAmmoCostPerShotAttribute(), EGameplayEffectAttributeCaptureSource::Source, true)
{
	RelevantAttributesToCapture.Add(AmmoCostCapture);
}

float UAscWeaponAmmoCostMagnitude::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	const AAscenduntWeapon* Weapon = Cast<AAscenduntWeapon>(Spec.GetContext().GetSourceObject());
	if (Weapon && Weapon->IsInfiniteMag())
	{
		return 0.f;
	}

	FAggregatorEvaluateParameters Parameters;
	Parameters.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	Parameters.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();
	float Cost = 0.f;
	GetCapturedAttributeMagnitude(AmmoCostCapture, Spec, Parameters, Cost);
	return -float(FMath::Max(0, FMath::RoundToInt(Cost)));
}

UAscWeaponAmmoCost::UAscWeaponAmmoCost()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UAscenduntWeaponSet::GetCurrentAmmoAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	FCustomCalculationBasedFloat Magnitude;
	Magnitude.CalculationClassMagnitude = UAscWeaponAmmoCostMagnitude::StaticClass();
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
	Modifiers.Add(Modifier);
}

UAscWeaponAmmoAdjustment::UAscWeaponAmmoAdjustment()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UAscenduntWeaponSet::GetCurrentAmmoAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat Magnitude;
	Magnitude.DataTag = AscenduntGameplayTags::Data_Weapon_AmmoDelta;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
	Modifiers.Add(Modifier);
}
