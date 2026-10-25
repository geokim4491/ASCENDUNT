#include "HakukoGameplayEffects.h"

#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntCharacterStatSet.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntWeaponSet.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"

namespace AscHakukoEffects
{
	static void AddSetByCallerMultiplier(UGameplayEffect* Effect, const FGameplayAttribute& Attribute, const FGameplayTag& DataTag)
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Multiplicitive;
		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = DataTag;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
		Effect->Modifiers.Add(Modifier);
	}
}

UHakukoStimGameplayEffect::UHakukoStimGameplayEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	AscHakukoEffects::AddSetByCallerMultiplier(this, UAscenduntCharacterStatSet::GetBaseMoveSpeedAttribute(), AscenduntGameplayTags::Data_Hakuko_Stim_MoveSpeedMultiplier);
	AscHakukoEffects::AddSetByCallerMultiplier(this, UAscenduntWeaponSet::GetRateOfFireAttribute(), AscenduntGameplayTags::Data_Hakuko_Stim_RateOfFireMultiplier);
	AscHakukoEffects::AddSetByCallerMultiplier(this, UAscenduntCharacterStatSet::GetDamageDealtScalarAttribute(), AscenduntGameplayTags::Data_Hakuko_Stim_DamageDealtMultiplier);
	AscHakukoEffects::AddSetByCallerMultiplier(this, UAscenduntCharacterStatSet::GetDamageTakenScalarAttribute(), AscenduntGameplayTags::Data_Hakuko_Stim_DamageTakenMultiplier);
}

UHakukoUltimateGameplayEffect::UHakukoUltimateGameplayEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	AscHakukoEffects::AddSetByCallerMultiplier(this, UAscenduntCharacterStatSet::GetDamageDealtScalarAttribute(), AscenduntGameplayTags::Data_Hakuko_Ultimate_DamageDealtMultiplier);
}
