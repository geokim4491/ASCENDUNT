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

enum class EAscenduntCueRoute : uint8;

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
	virtual bool FireOnce_Trigger(float DamageCoef, int32 ShotIndex, FAscenduntFireOnceContext& OutCtx, bool bDoCommit);

	/** 편의용: Auto/Trigger가 쉽게 쓰는 2인자 오버로드 */
	bool FireOnce_Trigger(float DamageCoef, int32 ShotIndex = 0)
	{
		FAscenduntFireOnceContext Dummy;
		return FireOnce_Trigger(DamageCoef, ShotIndex, Dummy, /*bDoCommit=*/true);
	}

protected:
	// ==========================
	// Delivery (실제 발사 실행부)
	// ==========================
	bool FireDelivery_Hitscan(const FAscenduntFireOnceContext& Ctx);
	bool FireDelivery_Projectile(const FAscenduntFireOnceContext& Ctx);

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
	virtual bool BuildViewRayAndMuzzle(
		const FGameplayAbilityActorInfo* ActorInfo,
		AAscenduntWeapon* Weapon,
		FVector& OutViewStart,
		FVector& OutViewDir,
		FVector& OutMuzzleLoc
	) const;

	// Main trace used by hitscan/projectile aim: evaluates friendly/enemy/heal/full-HP rules.
	bool MakeShotImpactDecision(const FAscenduntFireOnceContext& Ctx, const FVector& Start, const FVector& End, FAscShotImpactDecision& OutDecision) const;

	FVector ApplySpreadDeterministic(const FVector& BaseDir, float SpreadDeg, int32 Seed) const;

	// ==========================
	// Damage & Projectile Spawn
	// ==========================

	void SpawnProjectile_Server(const FAscenduntFireOnceContext& Ctx, const FVector& SpawnLoc, const FVector& Dir) const;

	/**
	 * 투사체 발사 방향을 정한다. 기본은 총구에서 조준점을 향하는 직선이다.
	 *
	 * 중력을 받는 투사체는 이 직선으로 쏘면 반드시 짧게 떨어지므로,
	 * 파생 클래스가 탄도 해를 계산해 덮어쓴다. UGA_AIFire_Auto 참고.
	 */
	virtual FVector ComputeProjectileLaunchDir(const FAscenduntFireOnceContext& Ctx,
		const FVector& MuzzleLoc, const FVector& AimPoint) const;

	// ==========================
	// Cues (최소 구현: GameplayCueTag로 실행)
	// ==========================
	void PostFireOnce(const FAscenduntFireOnceContext& Ctx,
		const FVector& FireStart, const FVector& FireEnd,
		bool bHasHit, const FHitResult* OptionalHit);

	void PlayFireCue(UAscenduntAbilitySystemComponent* InstigatorASC,
		const FVector& Start, const FVector& EndOrImpact,
		const FHitResult* Hit, EAscenduntCueRoute Route,
		bool bIsReplicated) const;

	// Hitscan Trail 전용 Cue. Start=Muzzle, End=각 Pellet의 최종 도착점.
	void PlayTraceCue(UAscenduntAbilitySystemComponent* InstigatorASC,
		const FVector& Start, const FVector& End,
		EAscenduntCueRoute Route, bool bIsReplicated) const;

	void PlayImpactWorldCue(UAscenduntAbilitySystemComponent* InstigatorASC,
		const FHitResult& Hit, EAscenduntCueRoute Route,
		bool bIsReplicated, bool bAOEImpact) const;

	void ApplyAOEDamage_Server(const FAscenduntFireOnceContext& Ctx, const FVector& ImpactPoint, AActor* DirectTarget) const;

protected:
	// ==========================
	// Config
	// ==========================
	UPROPERTY(EditDefaultsOnly, Category = "Fire|Trace")
	bool bUseControllerView = true;

	// Cue Tags (없으면 실행 안 함)
	UPROPERTY(EditDefaultsOnly, Category = "Fire|Cues")
	FGameplayTag FireCueTagReplicated;

	// Replicated와 같은 VFX지만, 로컬로 예측하는 버전임
	UPROPERTY(EditDefaultsOnly, Category = "Fire|Cues")
	FGameplayTag FireCueTagPredicted;

	UPROPERTY(EditDefaultsOnly, Category = "Fire|Cues")
	FGameplayTag TraceCueTagReplicated;

	UPROPERTY(EditDefaultsOnly, Category = "Fire|Cues")
	FGameplayTag TraceCueTagPredicted;

	UPROPERTY(EditDefaultsOnly, Category = "Fire|Cues")
	FGameplayTag ImpactWorldCueTagReplicated;

	UPROPERTY(EditDefaultsOnly, Category = "Fire|Cues")
	FGameplayTag ImpactWorldCueTagPredicted;

	// ==========================
	// Reload / Auto Reload
	// ==========================
	// GA_Reload의 AbilityTags 중 하나로 지정해두면 TryActivateAbilitiesByTag로 활성화됨
	UPROPERTY(VisibleDefaultsOnly, Category = "Fire|Reload")
	FGameplayTag ReloadAbilityTag;

	// Reload 활성화 시도. 성공하면(활성화됨) true.
	bool TryAutoReload(UAbilitySystemComponent* ASC, AAscenduntWeapon* Weapon, bool bEndThisAbilityBeforeReload);

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
