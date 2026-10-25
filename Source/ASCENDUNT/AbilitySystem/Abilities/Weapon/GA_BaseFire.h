// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/AbilitySystem/Abilities/Weapon/GA_WeaponAbilityBase.h"
#include "GA_BaseFire.generated.h"

class UAbilitySystemComponent;
class UAscenduntAbilitySystemComponent;
class UGameplayEffect;
class AAscenduntWeapon;
struct FAscenduntWeaponModeStats;
class UAbilityTask_PlayMontageAndWait;
class UAT_PlayWeaponMontageAndWait;

/**
 *
 */
UCLASS(Abstract)
class ASCENDUNT_API UGA_BaseFire : public UGA_WeaponAbilityBase
{
	GENERATED_BODY()

public:
	UGA_BaseFire();

	/** 게이팅 + (옵션) Commit + Context 채우기 */
	virtual bool TryPrepareShot(float DamageCoef, int32 ShotIndex, FAscenduntWeaponAttackContext& OutCtx, bool bCommitAbility);

	/** 편의용: Auto/Trigger가 쉽게 쓰는 2인자 오버로드 */
	bool TryPrepareShot(float DamageCoef, int32 ShotIndex = 0)
	{
		FAscenduntWeaponAttackContext Dummy;
		return TryPrepareShot(DamageCoef, ShotIndex, Dummy, /*bCommitAbility=*/true);
	}

protected:
	virtual bool CheckCost(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	    FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ApplyCost(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	    FGameplayAbilityActivationInfo ActivationInfo) const override;
	// ==========================
	// Delivery (실제 발사 실행부)
	// ==========================
	bool FireHitscan(const FAscenduntWeaponAttackContext& Ctx);
	bool FireProjectile(const FAscenduntWeaponAttackContext& Ctx);

	// ==========================
	// Helpers (Fire 전용 유틸)
	// ==========================
	int32 GetAmmoCostPerTrigger(const UAbilitySystemComponent* ASC) const;
	float GetShotIntervalSeconds(const UAbilitySystemComponent* ASC) const;
	float GetBaseSpreadDegSafe(const UAbilitySystemComponent* ASC) const;

	virtual int32 GetProjectilesPerTrigger(const UAbilitySystemComponent* ASC) const;

	// Hitscan origin/dir (muzzle, controller view dir)
	//
	// virtual 인 이유: AI 는 PlayerController 가 없어 이 함수의 기본 구현이
	// 액터 회전(Yaw 전용)을 조준선으로 쓴다. 상하 조준과 무빙 사격이 필요한 AI 는
	// UGA_AIFire_Auto 에서 이 함수만 갈아끼운다.
	// 기본 구현은 그대로이므로 히어로 경로는 영향을 받지 않는다.
	virtual bool BuildViewRayAndMuzzle(const FGameplayAbilityActorInfo* ActorInfo, AAscenduntWeapon* Weapon, FVector& OutViewStart, FVector& OutViewDir,
	    FVector& OutMuzzleLoc) const;

	// Main trace used by hitscan/projectile aim: evaluates friendly/enemy/heal/full-HP rules.
	bool MakeShotImpactDecision(const FAscenduntWeaponAttackContext& Ctx, const FVector& Start, const FVector& End, FAscShotImpactDecision& OutDecision) const;

	FVector ApplySpreadDeterministic(const FVector& BaseDir, float SpreadDeg, int32 Seed) const;

	// ==========================
	// Damage & Projectile Spawn
	// ==========================

	bool SpawnProjectile_Server(const FAscenduntWeaponAttackContext& Ctx, const FVector& SpawnLoc, const FVector& Dir) const;

	/**
	 * 투사체 발사 방향을 정한다. 기본은 총구에서 조준점을 향하는 직선이다.
	 *
	 * 중력을 받는 투사체는 이 직선으로 쏘면 반드시 짧게 떨어지므로,
	 * 파생 클래스가 탄도 해를 계산해 덮어쓴다. UGA_AIFire_Auto 참고.
	 */
	virtual FVector ComputeProjectileLaunchDir(const FAscenduntWeaponAttackContext& Ctx, const FVector& MuzzleLoc, const FVector& AimPoint) const;

	// ==========================
	// Cues (최소 구현: GameplayCueTag로 실행)
	// ==========================
	void FinalizeShot(const FAscenduntWeaponAttackContext& Ctx, const FVector& FireStart, const FVector& FireEnd, bool bHasHit, const FHitResult* OptionalHit);

	void PlayFireCue(UAscenduntAbilitySystemComponent* InstigatorASC, const FVector& Start, const FVector& EndOrImpact, const FHitResult* Hit,
	    float ShotInterval) const;

	// Hitscan Trail 전용 Cue. Start=Muzzle, End=각 Pellet의 최종 도착점.
	void PlayTraceCue(UAscenduntAbilitySystemComponent* InstigatorASC, const FVector& Start, const FVector& End, bool bUnusedLegacyRoute) const;

	void PlayImpactWorldCue(UAscenduntAbilitySystemComponent* InstigatorASC, const FHitResult& Hit, bool bIsReplicated, bool bAOEImpact) const;

	void ApplyAOEDamage_Server(const FAscenduntWeaponAttackContext& Ctx, const FVector& ImpactPoint, AActor* DirectTarget) const;

protected:
	// ==========================
	// Config
	// ==========================
	UPROPERTY(EditDefaultsOnly, Category = "Fire|Trace")
	bool bUseControllerView = true;

	UPROPERTY(EditDefaultsOnly, Category = "Fire|Cues")
	FGameplayTag FireCueTag;

	UPROPERTY(EditDefaultsOnly, Category = "Fire|Cues")
	FGameplayTag TraceCueTag;

	UPROPERTY(EditDefaultsOnly, Category = "Fire|Cues")
	FGameplayTag WorldImpactCueTag;

	// ==========================
	// Reload / Auto Reload
	// ==========================
	// GA_Reload의 AbilityTags 중 하나로 지정해두면 TryActivateAbilitiesByTag로 활성화됨
	UPROPERTY(VisibleDefaultsOnly, Category = "Fire|Reload")
	FGameplayTag ReloadAbilityTag;

	// Reload 활성화 시도. 성공하면(활성화됨) true.
	bool TryAutoReload(UAbilitySystemComponent* ASC, AAscenduntWeapon* Weapon);

	// Projectile defaults (ModeStats에 없으면 fallback)
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float DefaultProjectileSpeed = 2500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float DefaultProjectileScale = 1.f;

	UPROPERTY()
	TObjectPtr<UAT_PlayWeaponMontageAndWait> WeaponMontageTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> HeroMontageTask;
};
