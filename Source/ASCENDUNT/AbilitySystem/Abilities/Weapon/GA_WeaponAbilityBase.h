// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/AbilitySystem/Abilities/AscenduntGameplayAbility.h"
#include "GA_WeaponAbilityBase.generated.h"

class UAbilitySystemComponent;
class UAscenduntAbilitySystemComponent;
class UGameplayEffect;
class AAscenduntWeapon;

// =====================================================
// Weapon impact decision: "what does this weapon attack actually affect?"
// - WorldBlock: stop at world (blocking hit)
// - DamageTarget: first enemy ASC hit
// - HealTarget: first friendly ASC hit that is NOT full health
// =====================================================
enum class EAscShotImpactKind : uint8
{
	None = 0,
	WorldBlock,
	DamageTarget,
	HealTarget
};

struct FAscShotImpactDecision
{
	EAscShotImpactKind Kind = EAscShotImpactKind::None;
	FHitResult Hit;
	TWeakObjectPtr<UAbilitySystemComponent> TargetASC;
};

// Non-owning attack snapshot. Resolve object references again before delayed use.
struct FAscenduntWeaponAttackContext
{
	TWeakObjectPtr<UAscenduntAbilitySystemComponent> AscASC;
	TWeakObjectPtr<AAscenduntWeapon> Weapon;

	int32 AmmoCost = 0;
	double Now = 0.0;
	float Interval = 0.f;
	bool bAuthority = false;

	float DamageCoef = 1.f;

	// 0 이상이면 Data_DamageOverride로 전달되어 BaseDamage 대신 이 값을 사용.
	// -1이면 기존 BaseDamage * DamageCoef 경로 유지.
	float DamageOverride = -1.f;

	float Instances = 1.f;

	int32 ShotIndex = 0;

	// Snapshot at trigger time. Hero-specific state is queried once so impact/cue results remain deterministic.
	bool bAOEImpact = false;
	float AOERadius = 0.f;
	float AOEDamageCoefficient = 1.f;
	bool bDebugDrawAOERadius = false;
	float AOEDebugDrawDuration = 0.f;
};

/**
 * Fire/Melee 공통 무기 Ability 베이스.
 * 무기/ASC 조회, 공통 데미지/힐 GE 적용, TraceChannel 같은 무기 공통 요소만 둔다.
 * Fire 전용 탄약/발사체/히트스캔 로직은 UGA_BaseFire에 남긴다.
 */
UCLASS(Abstract)
class ASCENDUNT_API UGA_WeaponAbilityBase : public UAscenduntGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_WeaponAbilityBase();

protected:
	bool GetASCAndWeapon(UAbilitySystemComponent*& OutASC, UAscenduntAbilitySystemComponent*& OutAscASC, AAscenduntWeapon*& OutWeapon) const;

	// Applies gameplay effects based on decision. Team/full-HP filtering must be done by the caller.
	void ApplyImpactDecision_Server(const FAscenduntWeaponAttackContext& Ctx, const FAscShotImpactDecision& Decision, const FVector& TraceStart) const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Heal")
	TSubclassOf<UGameplayEffect> HealEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel;
};
