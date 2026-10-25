#pragma once

#include "NativeGameplayTags.h"

namespace AscenduntGameplayTags
{
	FGameplayTag FindTagByString(const FString& TagString, bool bMatchPartialString = false);


	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Move);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Look_Mouse);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_UI_CharacterSelection);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Interact);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_ADS);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Scroll_Up);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Scroll_Down);


	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Death);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Death_Dying);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Death_Dead);

	// 사망 연출 어빌리티를 트리거하는 게임플레이 이벤트.
	// C++에서 직접 발송하므로 네이티브로 선언한다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Death);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Weapon_Firing);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Weapon_Melee);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Weapon_Reloading);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Weapon_ADS);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Invincible);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_PvE_Downed);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Interacting);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_SpawnRoom);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Ultimate_Active);

	// Active GameplayEffects can opt into lifecycle cleanup with these granted tags.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_RemoveOnDeath);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Revive);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_RemoveOnHeroChange);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Hakuko_Fox_Attack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Hakuko_Fox_Support);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Hakuko_Ultimate);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Astell_Melee);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Astell_SwordWave);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Astell_DashSlash);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Astell_HolyExplosion);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Astell_Leap);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Astell_Ultimate);

	// Runtime-owned tags granted by Astell skill cooldown GameplayEffects.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Astell_SwordWave);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Astell_DashSlash);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Astell_HolyExplosion);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Astell_Leap);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Astell_Ultimate);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Hakuko_BioGrenade);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Hakuko_Stim);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Hakuko_HealGrenade);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Hakuko_SelectFox);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Hakuko_SendFox);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Hakuko_Dash);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Hakuko_Ultimate);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Hakuko_BioGrenade);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Hakuko_Stim);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Hakuko_HealGrenade);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stock_Astell_SwordWave);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stock_Astell_DashSlash);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Weapon_Fire);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Weapon_Melee);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Weapon_Reload);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Weapon_ADS);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Interact);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_Spawned);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_DataAvailable);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_DataInitialized);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_GASInitialized);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_GameplayReady);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Source_ThornReflect);      // Damage.Source.ThornReflect
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Policy_NoThornReflect);    // Damage.Policy.NoThornReflect
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Policy_IgnoreArmor);       // Damage.Policy.IgnoreArmor
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Policy_BypassOverHealth);  // Damage.Policy.BypassOverHealth
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Weapon_AOEImpact);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Weapon_AOESplash);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Fire);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Trace);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_ImpactWorld);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Weapon_AmmoDelta);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Replicated_Weapon_ImpactCharacter_Health);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Replicated_Weapon_ImpactCharacter_Shield);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Replicated_Weapon_ImpactCharacter_Heal);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Local_Weapon_ImpactCharacter_Heal);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Local_Weapon_ImpactCharacter);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Local_Weapon_Reload);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Replicated_Astell_HolyExplosion_Target);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Replicated_AI_DeathBurst);


	// SetByCaller 키
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Instances);  // Data.Instances
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_DamageOverride);  // Data.DamageOverride
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_DamageCoef);  // Data.DamageCoef
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_DamageFlat);  // Data.DamageFlat
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_HealOverride);  // Data.HealOverride

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Hakuko_Stim_MoveSpeedMultiplier);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Hakuko_Stim_RateOfFireMultiplier);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Hakuko_Stim_DamageDealtMultiplier);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Hakuko_Stim_DamageTakenMultiplier);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Hakuko_Ultimate_DamageDealtMultiplier);


	// Weapon stats
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Weapon_BaseDamage); // Data.Weapon.BaseDamage
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Weapon_BaseHeal); // Data.Weapon.BaseHeal
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Weapon_RateOfFire); // Data.Weapon.RateOfFire
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Weapon_ShotsPerTrigger); // Data.Weapon.ShotsPerTrigger
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Weapon_AmmoCostPerShot); // Data.Weapon.AmmoCostPerShot

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Weapon_InfiniteMag); // Data.Weapon.InfiniteMag
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Weapon_MagSize); // Data.Weapon.MagSize
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Weapon_ReloadTime); // Data.Weapon.ReloadTime

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Weapon_BaseSpreadDeg); // Data.Weapon.BaseSpreadDeg
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Weapon_SpreadIncPerShot); // Data.Weapon.SpreadIncPerShot
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Weapon_SpreadRecoverPerSec); // Data.Weapon.SpreadRecoverPerSec
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Weapon_RecoilPitch); // Data.Weapon.RecoilPitch
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Weapon_RecoilYaw); // Data.Weapon.RecoilYaw

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Weapon_AllowHeadshot); // Data.Weapon.AllowHeadshot
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Weapon_HeadshotDamageMultiplier); // Data.Weapon.HeadshotDamageMultiplier

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Weapon_MoveSpeedScalarWhileFiring); // Data.Weapon.MoveSpeedScalarWhileFiring
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Weapon_MoveSpeedScalarWhileADS); // Data.Weapon.MoveSpeedScalarWhileADS

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Weapon_ProjectileSpeed); // Data.Weapon.ProjectileSpeed
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Weapon_ProjectileScale); // Data.Weapon.ProjectileScale
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Weapon_ProjectileCanBeNullified); // Data.Weapon.ProjectileCanBeNullified

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Weapon_MeleePlayRate); // Data.Weapon.MeleePlayRate
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Weapon_MeleeDamageCoef); // Data.Weapon.MeleeDamageCoef

}
