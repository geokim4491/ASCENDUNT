#include "AscenduntGameplayTags.h"

#include "Engine/EngineTypes.h"
#include "GameplayTagsManager.h"

namespace AscenduntGameplayTags
{

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Move, "InputTag.Move", "Move input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Look_Mouse, "InputTag.Look", "Look (mouse) input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_UI_CharacterSelection, "InputTag.UI.CharacterSelection", "Open the in-game character selection UI.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Interact, "InputTag.Interact", "Common world interaction input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_ADS, "InputTag.ADS", "Aim down sights input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Scroll_Up, "InputTag.Scroll.Up", "Positive mouse-wheel ability input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Scroll_Down, "InputTag.Scroll.Down", "Negative mouse-wheel ability input.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Death, "Status.Death", "Target has the death status.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Death_Dying, "Status.Death.Dying", "Target has begun the death process.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Death_Dead, "Status.Death.Dead", "Target has finished the death process.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Death, "Event.Death", "Sent when a character's death sequence begins. Triggers the death presentation ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Weapon_Firing, "Status.Weapon.Firing", "Target Fire Weapon");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Weapon_Melee, "Status.Weapon.Melee", "Target Melee Weapon");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Weapon_Reloading, "Status.Weapon.Reloading", "Target Reloading Weapon");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Weapon_ADS, "Status.Weapon.ADS", "Owning local player is aiming down sights.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Invincible, "Status.Invincible", "Target cannot receive damage while this tag is active.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_PvE_Downed, "Status.PvE.Downed", "PvE hero is incapacitated and waiting for revive or bleedout.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Interacting, "Status.Interacting", "Hero is currently holding a world interaction.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_SpawnRoom, "Status.SpawnRoom", "Target is currently inside a spawn/respawn room volume.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Ultimate_Active, "Status.Ultimate.Active", "Owner currently has an ultimate ability active. Ultimate charge gain is suspended while this tag is present.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Effect_RemoveOnDeath, "Effect.Lifecycle.RemoveOnDeath", "Active effect is removed when the owner dies.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Effect_Revive, "Effect.Revive", "Healing/effects that are allowed to restore a PvE Downed hero.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Effect_RemoveOnHeroChange, "Effect.Lifecycle.RemoveOnHeroChange", "Active effect is removed when the selected hero changes.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Hakuko_Fox_Attack, "Status.Hakuko.Fox.Attack", "Target currently has Hakuko's attack fox.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Hakuko_Fox_Support, "Status.Hakuko.Fox.Support", "Target currently has Hakuko's support fox.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Hakuko_Ultimate, "Status.Hakuko.Ultimate", "Hakuko primary fire is currently empowered into AOE damage.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Astell_Melee, "Ability.Astell.Melee", "Astell melee combo.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Astell_SwordWave, "Ability.Astell.SwordWave", "Astell sword wave.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Astell_DashSlash, "Ability.Astell.DashSlash", "Astell dash slash.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Astell_HolyExplosion, "Ability.Astell.HolyExplosion", "Astell holy explosion.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Astell_Leap, "Ability.Astell.Leap", "Astell leap.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Astell_Ultimate, "Ability.Astell.Ultimate", "Astell ultimate holy sword.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Astell_SwordWave, "Cooldown.Astell.SwordWave", "Astell sword wave cooldown.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Astell_DashSlash, "Cooldown.Astell.DashSlash", "Astell dash slash cooldown.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Astell_HolyExplosion, "Cooldown.Astell.HolyExplosion", "Astell holy explosion cooldown.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Astell_Leap, "Cooldown.Astell.Leap", "Astell leap cooldown.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Astell_Ultimate, "Cooldown.Astell.Ultimate", "Astell ultimate cooldown.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Hakuko_BioGrenade, "Cooldown.Hakuko.BioGrenade", "Hakuko biotic grenade cooldown.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Hakuko_Stim, "Cooldown.Hakuko.Stim", "Hakuko stim cooldown.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Hakuko_HealGrenade, "Cooldown.Hakuko.HealGrenade", "Hakuko healing grenade cooldown.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Hakuko_SelectFox, "Ability.Hakuko.SelectFox", "Hakuko changes the selected fox.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Hakuko_SendFox, "Ability.Hakuko.SendFox", "Hakuko sends the selected fox to an ally.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Hakuko_Dash, "Ability.Hakuko.Dash", "Hakuko movement dash.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Hakuko_Ultimate, "Ability.Hakuko.Ultimate", "Hakuko empowers her rifle into AOE fire.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Hakuko_BioGrenade, "Ability.Hakuko.BioGrenade", "Hakuko biotic grenade.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Hakuko_Stim, "Ability.Hakuko.Stim", "Hakuko ally combat stimulant.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Hakuko_HealGrenade, "Ability.Hakuko.HealGrenade", "Hakuko secondary healing grenade.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stock_Astell_SwordWave, "Stock.Astell.SwordWave", "Astell sword wave stock.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stock_Astell_DashSlash, "Stock.Astell.DashSlash", "Astell dash slash stock.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Weapon_Fire, "Ability.Weapon.Fire", "Primary Fire");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Weapon_Melee, "Ability.Weapon.Melee", "Primary Melee");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Weapon_Reload, "Ability.Weapon.Reload", "Reload");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Weapon_ADS, "Ability.Weapon.ADS", "Weapon aim-down-sights ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Interact, "Ability.Interact", "Common world interaction ability.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InitState_Spawned, "InitState.Spawned", "1: Actor/component has initially spawned and can be extended");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InitState_DataAvailable, "InitState.DataAvailable", "2: All required data has been loaded/replicated and is ready for initialization");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InitState_DataInitialized, "InitState.DataInitialized", "3: The available data has been initialized for this actor/component, but it is not ready for full gameplay");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InitState_GASInitialized, "InitState.GASInitialized", "Hero: ASC ActorInfo is initialized and PawnData GAS content has been applied.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InitState_GameplayReady, "InitState.GameplayReady", "4: The actor/component is fully ready for active gameplay");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Source_ThornReflect, "Damage.Source.ThornReflect", "반사로 들어온 공격은 다시 반사하지 않는다.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Policy_NoThornReflect, "Damage.Policy.NoThornReflect", "반사를 무시하는 공격");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Policy_IgnoreArmor, "Damage.Policy.IgnoreArmor", "해당 태그를 가진 공격은 방어력을 무시하고 기본 체력처럼 데미지를 입힙");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Policy_BypassOverHealth, "Damage.Policy.BypassOverHealth", "해당 태그를 가진 공격은 OverHealth를 무시함");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Effect_Weapon_AOEImpact, "Effect.Weapon.AOEImpact", "Direct impact came from an AOE-enabled weapon shot and should use AOE impact presentation.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Effect_Weapon_AOESplash, "Effect.Weapon.AOESplash", "Damage was applied by the splash portion of an AOE weapon impact.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Predicted_Weapon_ImpactWorld, "GameplayCue.Predicted.Weapon.ImpactWorld", "벽, 구조물 등에 총알이 박힌 경우 GC");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Replicated_Weapon_ImpactWorld, "GameplayCue.Replicated.Weapon.ImpactWorld", "벽, 구조물 등에 총알이 박힌 경우 GC");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Predicted_Weapon_Fire, "GameplayCue.Predicted.Weapon.Fire", "총 발사 GC");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Replicated_Weapon_Fire, "GameplayCue.Replicated.Weapon.Fire", "총 발사 GC");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Predicted_Weapon_Trace, "GameplayCue.Predicted.Weapon.Trace", "히트스캔 탄도 Trail GC");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Replicated_Weapon_Trace, "GameplayCue.Replicated.Weapon.Trace", "히트스캔 탄도 Trail GC");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Replicated_Weapon_ImpactCharacter_Health, "GameplayCue.Replicated.Weapon.ImpactCharacter.Health", "체력, Thron이 깎일 때 GC");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Replicated_Weapon_ImpactCharacter_Shield, "GameplayCue.Replicated.Weapon.ImpactCharacter.Shield", "Shield 깎일 때 GC");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Replicated_Weapon_ImpactCharacter_Heal, "GameplayCue.Replicated.Weapon.ImpactCharacter.Heal", "Heal 줄 때 GC");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Local_Weapon_ImpactCharacter_Heal, "GameplayCue.Local.Weapon.ImpactCharacter.Heal", "Heal 줄 때 Local GC");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Local_Weapon_ImpactCharacter, "GameplayCue.Local.Weapon.ImpactCharacter", "로컬로 실행되는 이펙트 히트 마커 등");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Local_Weapon_Reload, "GameplayCue.Local.Weapon.Reload", "재장전 GC");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Replicated_Astell_HolyExplosion_Target, "GameplayCue.Replicated.Astell.HolyExplosion.Target", "Astell Holy Explosion burst attached to each affected target.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Replicated_AI_DeathBurst, "GameplayCue.Replicated.AI.DeathBurst", "AI 사망 시 파편 폭발 GC");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Instances, "Data.Instances", "SetByCaller");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_DamageOverride, "Data.DamageOverride", "SetByCaller");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_DamageCoef, "Data.DamageCoef", "SetByCaller");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_DamageFlat, "Data.DamageFlat", "SetByCaller");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_HealOverride, "Data.HealOverride", "SetByCaller");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Hakuko_Stim_MoveSpeedMultiplier, "Data.Hakuko.Stim.MoveSpeedMultiplier", "Stim SetByCaller multiplier.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Hakuko_Stim_RateOfFireMultiplier, "Data.Hakuko.Stim.RateOfFireMultiplier", "Stim SetByCaller multiplier.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Hakuko_Stim_DamageDealtMultiplier, "Data.Hakuko.Stim.DamageDealtMultiplier", "Stim SetByCaller multiplier.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Hakuko_Stim_DamageTakenMultiplier, "Data.Hakuko.Stim.DamageTakenMultiplier", "Stim SetByCaller multiplier.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Hakuko_Ultimate_DamageDealtMultiplier, "Data.Hakuko.Ultimate.DamageDealtMultiplier", "Ultimate SetByCaller outgoing damage multiplier.");


	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Weapon_BaseDamage, "Data.Weapon.BaseDamage", "SetByCaller");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Weapon_BaseHeal, "Data.Weapon.BaseHeal", "SetByCaller");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Weapon_RateOfFire, "Data.Weapon.RateOfFire", "SetByCaller");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Weapon_ShotsPerTrigger, "Data.Weapon.ShotsPerTrigger", "SetByCaller");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Weapon_AmmoCostPerShot, "Data.Weapon.AmmoCostPerShot", "SetByCaller");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Weapon_InfiniteMag, "Data.Weapon.InfiniteMag", "SetByCaller");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Weapon_MagSize, "Data.Weapon.MagSize", "SetByCaller");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Weapon_ReloadTime, "Data.Weapon.ReloadTime", "SetByCaller");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Weapon_BaseSpreadDeg, "Data.Weapon.BaseSpreadDeg", "SetByCaller");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Weapon_SpreadIncPerShot, "Data.Weapon.SpreadIncPerShot", "SetByCaller");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Weapon_SpreadRecoverPerSec, "Data.Weapon.SpreadRecoverPerSec", "SetByCaller");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Weapon_RecoilPitch, "Data.Weapon.RecoilPitch", "SetByCaller");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Weapon_RecoilYaw, "Data.Weapon.RecoilYaw", "SetByCaller");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Weapon_AllowHeadshot, "Data.Weapon.AllowHeadshot", "SetByCaller");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Weapon_HeadshotDamageMultiplier, "Data.Weapon.HeadshotDamageMultiplier", "SetByCaller");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Weapon_MoveSpeedScalarWhileFiring, "Data.Weapon.MoveSpeedScalarWhileFiring", "SetByCaller");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Weapon_MoveSpeedScalarWhileADS, "Data.Weapon.MoveSpeedScalarWhileADS", "SetByCaller");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Weapon_ProjectileSpeed, "Data.Weapon.ProjectileSpeed", "SetByCaller");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Weapon_ProjectileScale, "Data.Weapon.ProjectileScale", "SetByCaller");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Weapon_ProjectileCanBeNullified, "Data.Weapon.ProjectileCanBeNullified", "SetByCaller");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Weapon_MeleePlayRate, "Data.Weapon.MeleePlayRate", "SetByCaller");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Weapon_MeleeDamageCoef, "Data.Weapon.MeleeDamageCoef", "SetByCaller");

	FGameplayTag FindTagByString(const FString& TagString, bool bMatchPartialString)
	{
		const UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
		FGameplayTag Tag = Manager.RequestGameplayTag(FName(*TagString), false);

		if (!Tag.IsValid() && bMatchPartialString)
		{
			FGameplayTagContainer AllTags;
			Manager.RequestAllGameplayTags(AllTags, true);

			for (const FGameplayTag& TestTag : AllTags)
			{
				if (TestTag.ToString().Contains(TagString))
				{
					UE_LOG(LogTemp, Display, TEXT("Could not find exact match for tag [%s] but found partial match on tag [%s]."), *TagString, *TestTag.ToString());
					Tag = TestTag;
					break;
				}
			}
		}

		return Tag;
	}

}
