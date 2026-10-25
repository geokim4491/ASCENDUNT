#include "AscAbilityData.h"

#include "ASCENDUNT/AbilitySystem/Effects/HakukoGameplayEffects.h"
#include "ASCENDUNT/Character/Hero/Hakuko/HakukoGrenadeProjectile.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"
#include "UObject/ConstructorHelpers.h"
#include "GameplayTagsManager.h"



void UAscAbilityData::PostLoad()
{
	Super::PostLoad();

	// Backward-compatible generic migration: Ability.Hero.Skill -> Cooldown.Hero.Skill.
	// Explicit CooldownTag always wins, and no hero-specific mapping lives in GA code anymore.
	if (!CooldownTag.IsValid() && CooldownDuration > 0.0f && AbilityTag.IsValid())
	{
		FString AbilityName = AbilityTag.ToString();
		if (AbilityName.RemoveFromStart(TEXT("Ability.")))
		{
			const FName CandidateName(*FString::Printf(TEXT("Cooldown.%s"), *AbilityName));
			CooldownTag = FGameplayTag::RequestGameplayTag(CandidateName, false);
		}
	}
}

namespace AscHakukoDataDefaults
{
	static TSubclassOf<UGameplayEffect> FindDamageEffect()
	{
		static ConstructorHelpers::FClassFinder<UGameplayEffect> Finder(TEXT("/Game/AbilitySystem/GE/GE_Damage_Instant"));
		return Finder.Succeeded() ? Finder.Class : nullptr;
	}
	static TSubclassOf<UGameplayEffect> FindHealEffect()
	{
		static ConstructorHelpers::FClassFinder<UGameplayEffect> Finder(TEXT("/Game/AbilitySystem/GE/GE_Heal_Instant"));
		return Finder.Succeeded() ? Finder.Class : nullptr;
	}
}

UHakukoBioGrenadeData::UHakukoBioGrenadeData()
{
	AbilityTag = AscenduntGameplayTags::Ability_Hakuko_BioGrenade;
	CooldownTag = AscenduntGameplayTags::Cooldown_Hakuko_BioGrenade;
	Projectile.ProjectileClass = AHakukoGrenadeProjectile::StaticClass();
	DamageEffectClass = AscHakukoDataDefaults::FindDamageEffect();
	HealEffectClass = AscHakukoDataDefaults::FindHealEffect();
}

UHakukoHealGrenadeData::UHakukoHealGrenadeData()
{
	AbilityTag = AscenduntGameplayTags::Ability_Hakuko_HealGrenade;
	CooldownTag = AscenduntGameplayTags::Cooldown_Hakuko_HealGrenade;
	Projectile.ProjectileClass = AHakukoGrenadeProjectile::StaticClass();
	HealEffectClass = AscHakukoDataDefaults::FindHealEffect();
}

UHakukoStimData::UHakukoStimData()
{
	AbilityTag = AscenduntGameplayTags::Ability_Hakuko_Stim;
	CooldownTag = AscenduntGameplayTags::Cooldown_Hakuko_Stim;
	StimEffectClass = UHakukoStimGameplayEffect::StaticClass();
}

UHakukoUltimateData::UHakukoUltimateData()
{
	AbilityTag = AscenduntGameplayTags::Ability_Hakuko_Ultimate;
	CooldownDuration = 0.0f;
	CooldownTag = FGameplayTag();
	BuffEffectClass = UHakukoUltimateGameplayEffect::StaticClass();
}
