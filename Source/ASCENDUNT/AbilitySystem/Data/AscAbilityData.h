#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "AscAbilityData.generated.h"

class AAscenduntProjectile;
class UAnimMontage;
class UGameplayEffect;
class UTexture2D;

USTRUCT(BlueprintType)
struct FAscMontageData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Montage")
	TObjectPtr<UAnimMontage> HeroMontage = nullptr;

	// Local-owner first-person arms montage. ServerOnly abilities multicast the 3P presentation
	// from the server and only the owning client plays this montage.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Montage")
	TObjectPtr<UAnimMontage> HeroMontage1P = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Montage")
	TObjectPtr<UAnimMontage> WeaponMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Montage")
	FName StartSection = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Montage", meta = (ClampMin = "0.01"))
	float PlayRate = 1.0f;

	// If true, this DataAbility stays active until the primary montage ends.
	// If false, the ability may end immediately after its gameplay effect is applied while the montage keeps playing.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Montage")
	bool bEndAbilityWhenMontageEnds = true;

};

UCLASS(Abstract, BlueprintType)
class ASCENDUNT_API UAscAbilityData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual void PostLoad() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	FGameplayTag AbilityTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	FAscMontageData Montage;

	// Skill icon used by the shared HUD ability-slot widget.
	// Configure this once on each ability DataAsset instead of assigning images per widget slot.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TObjectPtr<UTexture2D> AbilityIcon = nullptr;

	// Base cooldown in seconds. CommitAbility() applies this through GAS.
	// 0 disables the cooldown for this ability.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cooldown", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float CooldownDuration = 0.0f;

	// Runtime cooldown identity. Keeping this on the DataAsset avoids character-specific
	// hard coding in UGA_DataAbility and lets every hero define its own cooldown tag.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cooldown")
	FGameplayTag CooldownTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	TSubclassOf<UGameplayEffect> CostEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue")
	FGameplayTag GameplayCueTag;
};

USTRUCT(BlueprintType)
struct FHakukoGrenadeProjectileConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile")
	TSubclassOf<class AHakukoGrenadeProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta=(ClampMin="0.0"))
	float ProjectileSpeed = 1600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile")
	float UpwardVelocity = 250.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta=(ClampMin="0.0"))
	float GravityScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta=(ClampMin="0.01"))
	float ProjectileScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta=(ClampMin="1.0"))
	float CollisionRadius = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta=(ClampMin="0.05"))
	float ActiveDuration = 5.0f;

	// Local-space offset from the character/view basis. X=forward, Y=right, Z=up.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile")
	FVector SpawnOffset = FVector(70.0f, 25.0f, 20.0f);

	// Draw the live projectile collision sphere on clients while the grenade exists.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile|Debug")
	bool bDebugDrawProjectileCollision = false;

	// Draw the final AOE sphere at the detonation point on all clients.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile|Debug")
	bool bDebugDrawExplosionRadius = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile|Debug", meta=(ClampMin="0.0"))
	float DebugDrawDuration = 2.0f;
};

UCLASS(BlueprintType)
class ASCENDUNT_API UHakukoBioGrenadeData : public UAscAbilityData
{
	GENERATED_BODY()
public:
	UHakukoBioGrenadeData();
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Grenade")
	FHakukoGrenadeProjectileConfig Projectile;

	// Delay from ability activation/montage start until the biotic grenade is actually spawned.
	// This is intentionally Bio Grenade-only; the secondary heal grenade fires immediately.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Grenade", meta=(ClampMin="0.0", UIMin="0.0", Units="s"))
	float ThrowDelay = 0.25f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Grenade", meta=(ClampMin="0.0"))
	float ExplosionRadius = 400.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Grenade", meta=(ClampMin="0.0"))
	float Damage = 60.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Grenade", meta=(ClampMin="0.0"))
	float Heal = 60.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Grenade")
	TSubclassOf<UGameplayEffect> DamageEffectClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Grenade")
	TSubclassOf<UGameplayEffect> HealEffectClass;
};

UCLASS(BlueprintType)
class ASCENDUNT_API UHakukoStimData : public UAscAbilityData
{
	GENERATED_BODY()
public:
	UHakukoStimData();
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Stim", meta=(ClampMin="0.0"))
	float Duration = 5.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Stim", meta=(ClampMin="0.0"))
	float MoveSpeedMultiplier = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Stim", meta=(ClampMin="0.0"))
	float RateOfFireMultiplier = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Stim", meta=(ClampMin="0.0"))
	float DamageDealtMultiplier = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Stim", meta=(ClampMin="0.0"))
	float DamageTakenMultiplier = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Stim")
	TSubclassOf<UGameplayEffect> StimEffectClass;
};

UCLASS(BlueprintType)
class ASCENDUNT_API UHakukoHealGrenadeData : public UAscAbilityData
{
	GENERATED_BODY()
public:
	UHakukoHealGrenadeData();
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Heal Grenade")
	FHakukoGrenadeProjectileConfig Projectile;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Heal Grenade", meta=(ClampMin="0.0"))
	float ExplosionRadius = 350.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Heal Grenade", meta=(ClampMin="0.0"))
	float MinHeal = 10.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Heal Grenade", meta=(ClampMin="0.0"))
	float MaxHeal = 100.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Heal Grenade")
	TSubclassOf<UGameplayEffect> HealEffectClass;
};

UCLASS(BlueprintType)
class ASCENDUNT_API UHakukoUltimateData : public UAscAbilityData
{
	GENERATED_BODY()
public:
	UHakukoUltimateData();
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Ultimate", meta=(ClampMin="0.0"))
	float Duration = 8.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Ultimate", meta=(ClampMin="0.0"))
	float DamageDealtMultiplier = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Ultimate", meta=(ClampMin="0.0"))
	float AOERadius = 300.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Ultimate", meta=(ClampMin="0.0"))
	float AOEDamageCoefficient = 1.0f;

	/** Draws the actual weapon AOE sphere at each impact while the ultimate is active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Ultimate|Debug")
	bool bDebugDrawAOERadius = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Ultimate|Debug",
		meta=(EditCondition="bDebugDrawAOERadius", ClampMin="0.0"))
	float AOEDebugDrawDuration = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Ultimate")
	TSubclassOf<UGameplayEffect> BuffEffectClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Ultimate")
	TObjectPtr<class UNiagaraSystem> WeaponVFX = nullptr;
};

UCLASS(BlueprintType)
class ASCENDUNT_API UAstellSwordWaveData : public UAscAbilityData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SwordWave")
	TSubclassOf<AAscenduntProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SwordWave", meta = (ClampMin = "1"))
	int32 MaxStock = 2;

	// One use starts a refill timer. When the timer finishes, stock returns to MaxStock.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SwordWave", meta = (ClampMin = "0.0"))
	float RefillToMaxTime = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SwordWave")
	FGameplayTag StockTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.0"))
	float ProjectileSpeed = 2500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.01"))
	float ProjectileScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.0"))
	float ActiveDuration = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "-1.0"))
	float DamageOverride = -1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float DamageCoef = 1.0f;
};

UCLASS(BlueprintType)
class ASCENDUNT_API UAstellDashData : public UAscAbilityData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stock")
	FGameplayTag StockTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stock", meta = (ClampMin = "1"))
	int32 MaxStock = 2;

	// Dash slash uses normal charge recovery by default: each timer restores one stock.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stock", meta = (ClampMin = "0.0"))
	float RecoverOneTime = 8.0f;

	// Body-centered hit box. Movement is handled only by the animation montage/root motion.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit")
	FVector HitBoxHalfExtent = FVector(120.0f, 80.0f, 100.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit")
	FVector HitBoxLocalOffset = FVector(0.0f, 0.0f, 50.0f);

	// Montage time when the body hit box starts. This value is divided by Montage.PlayRate at runtime.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit|Timing", meta = (ClampMin = "0.0"))
	float HitStartTime = 0.0f;

	// Montage time when the body hit box ends. This value is divided by Montage.PlayRate at runtime.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit|Timing", meta = (ClampMin = "0.0"))
	float HitEndTime = 0.18f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit|Timing", meta = (ClampMin = "0.005"))
	float HitTickInterval = 0.02f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit|Debug")
	bool bDrawDebugHitBox = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit|Debug", meta = (ClampMin = "0.0"))
	float DebugDrawDuration = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit|Debug", meta = (ClampMin = "0.0"))
	float DebugDrawThickness = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "-1.0"))
	float DamageOverride = -1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float DamageCoef = 1.0f;
};

UCLASS(BlueprintType)
class ASCENDUNT_API UAstellExplosionData : public UAscAbilityData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Explosion", meta = (ClampMin = "0.0"))
	float Radius = 450.0f;

	// Montage time when damage is applied. Divided by Montage.PlayRate at runtime.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Explosion|Timing", meta = (ClampMin = "0.0"))
	float DamageApplyTime = 0.0f;

	// Montage time when knockback is applied. Divided by Montage.PlayRate at runtime.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Explosion|Timing", meta = (ClampMin = "0.0"))
	float KnockbackApplyTime = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Explosion", meta = (ClampMin = "0.0"))
	float KnockbackStrength = 900.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Explosion|Debug")
	bool bDrawDebugRadius = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Explosion|Debug", meta = (EditCondition = "bDrawDebugRadius", ClampMin = "0.0"))
	float DebugDrawDuration = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Explosion|Debug", meta = (EditCondition = "bDrawDebugRadius", ClampMin = "0.0"))
	float DebugDrawThickness = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	TSubclassOf<UGameplayEffect> BaseDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "-1.0"))
	float BaseDamageOverride = -1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float BaseDamageCoef = 1.0f;

};

UCLASS(BlueprintType)
class ASCENDUNT_API UAstellLeapData : public UAscAbilityData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Leap", meta = (ClampMin = "0.0"))
	float ForwardStrength = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Leap", meta = (ClampMin = "0.0"))
	float UpwardStrength = 900.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit", meta = (ClampMin = "0.0"))
	float HitRadius = 250.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit", meta = (ClampMin = "0.0"))
	float KnockbackStrength = 800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "-1.0"))
	float DamageOverride = -1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float DamageCoef = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Leap", meta = (ClampMin = "0.05"))
	float GroundCheckInterval = 0.1f;
};

UCLASS(BlueprintType)
class ASCENDUNT_API UAstellUltData : public UAscAbilityData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ultimate")
	TSubclassOf<UGameplayEffect> BuffEffectClass;
};
