// Fill out your copyright notice in the Description page of Project Settings.


#include "WeaponDefinitionData.h"
#include "GameplayEffect.h"

UWeaponDefinitionData::UWeaponDefinitionData(const FObjectInitializer& ObjectInitializer)
{
	static ConstructorHelpers::FClassFinder<UGameplayEffect> StatsGEClass(
		TEXT("/Game/AbilitySystem/GE/GE_WeaponStats_Runtime") // Blueprint asset path
	);

	if (StatsGEClass.Succeeded())
	{
		WeaponStatsEffect = StatsGEClass.Class; // UClass* 들어감
	}
}


void UWeaponDefinitionData::PostLoad()
{
	Super::PostLoad();

	// Migrate the previous individual impact fields into the paired ImpactVFX struct.
	for (FAscenduntWeaponModeConfig& Mode : Modes)
	{
		FAscenduntWeaponModeStats& Stats = Mode.Stats;

		if (!Stats.ImpactVFX.WorldNiagara && Stats.FireImpactWorldVFXNiagara)
		{
			Stats.ImpactVFX.WorldNiagara = Stats.FireImpactWorldVFXNiagara;
		}
		if (!Stats.ImpactVFX.WorldParticle && Stats.FireImpactWorldVFXParticle)
		{
			Stats.ImpactVFX.WorldParticle = Stats.FireImpactWorldVFXParticle;
		}

		UNiagaraSystem* LegacyCharacterNiagara = Stats.WeaponImpactCharacterVFXNiagara
			? Stats.WeaponImpactCharacterVFXNiagara.Get()
			: Stats.FireImpactCharacterVFXNiagara_DEPRECATED.Get();
		UParticleSystem* LegacyCharacterParticle = Stats.WeaponImpactCharacterVFXParticle
			? Stats.WeaponImpactCharacterVFXParticle.Get()
			: Stats.FireImpactCharacterVFXParticle_DEPRECATED.Get();

		if (!Stats.ImpactVFX.CharacterNiagara && LegacyCharacterNiagara)
		{
			Stats.ImpactVFX.CharacterNiagara = LegacyCharacterNiagara;
		}
		if (!Stats.ImpactVFX.CharacterParticle && LegacyCharacterParticle)
		{
			Stats.ImpactVFX.CharacterParticle = LegacyCharacterParticle;
		}

		// Keep the legacy reflected fields so existing Blueprint nodes still compile, but clear their runtime values.
		// Native impact cues now consume ImpactVFX/AOEImpactVFX; clearing prevents a legacy BP shell from spawning duplicates.
		Stats.FireImpactWorldVFXNiagara = nullptr;
		Stats.FireImpactWorldVFXParticle = nullptr;
		Stats.WeaponImpactCharacterVFXNiagara = nullptr;
		Stats.WeaponImpactCharacterVFXParticle = nullptr;
		Stats.FireImpactCharacterVFXNiagara_DEPRECATED = nullptr;
		Stats.FireImpactCharacterVFXParticle_DEPRECATED = nullptr;
	}
}
