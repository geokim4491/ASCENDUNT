// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameplayTagContainer.h"

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WeaponDefinitionData.generated.h"

class UAscenduntAbilitySet;
class UCurveFloat;
class UGameplayEffect;
class AAscenduntProjectile;
class UNiagaraSystem;
class UParticleSystem;
class USoundCue;
class UAnimMontage;

UENUM(BlueprintType)
enum class EAscenduntWeaponFireType : uint8
{
	Hitscan,
	Projectile,
	Melee
};

// =============================
// Melee trace profile
// Fixed forward box only. Legacy trace modes (BoneAttachedBox / SocketTrailSpheres) are intentionally removed.
// =============================
USTRUCT(BlueprintType)
struct FAscenduntMeleeTraceProfile
{
	GENERATED_BODY()

	// 윈도우 동안 몇 초마다 판정할지. 0 이하면 HitWindow Begin 때 1회만 판정.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee|Trace", meta = (ClampMin = "0.0"))
	float HitTickInterval = 0.02f;

	// 캐릭터 전방 고정 박스 크기.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee|Trace")
	FVector Fixed_BoxHalfExtent = FVector(60.f, 35.f, 55.f);

	// 캐릭터 기준 로컬 오프셋. X=전방, Y=우측, Z=상단.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee|Trace")
	FVector Fixed_LocalOffset = FVector(110.f, 0.f, 60.f);

	// 에디터/데이터 기반 근접 판정 디버그. 전역 CVar asc.DebugMelee=1로도 강제 표시 가능.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee|Debug")
	bool bDrawDebugHitBox = false;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee|Debug", meta = (EditCondition = "bDrawDebugHitBox", ClampMin = "0.0"))
	float DebugDrawDuration = 0.05f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee|Debug", meta = (EditCondition = "bDrawDebugHitBox", ClampMin = "0.1"))
	float DebugDrawThickness = 1.5f;
};

// =============================
// Melee step (몽타주 + 트레이스 프로파일)
// =============================
USTRUCT(BlueprintType)
struct FAscenduntMeleeStep
{
	GENERATED_BODY()

	// 히어로(팔/바디) 몽타주
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee")
	TObjectPtr<UAnimMontage> HeroMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee")
	TObjectPtr<UAnimMontage> HeroMontage1P = nullptr;

	// 무기 몽타주(검 전용 등) - 없어도 됨
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee")
	TObjectPtr<UAnimMontage> WeaponMontage = nullptr;

	// 이 스텝의 캐릭터/무기 몽타주 재생 속도 배율.
	// Weapon Mode의 MeleePlayRate와 곱해져 최종 재생 속도를 만든다.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee|Animation", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.0f;

	// 다음 콤보 입력을 받기 시작하는 시간. 현재 Step 시작 시점 기준.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee|Combo", meta = (ClampMin = "0.0"))
	float ComboInputOpenTime = 0.15f;

	// 다음 콤보 입력을 받는 마지막 시간. 현재 Step 시작 시점 기준.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee|Combo", meta = (ClampMin = "0.0"))
	float ComboInputCloseTime = 0.65f;

	// 이 스텝의 트레이스 프로파일
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee")
	FAscenduntMeleeTraceProfile TraceProfile;

	// 스텝별 DamageCoef(콤보 2타 강화 같은 거)
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee", meta = (ClampMin = "0.0"))
	float DamageCoef = 1.f;

	// 스텝별 절대 데미지. 0 이상이면 BaseDamage를 무시하고 이 값을 사용.
	// -1 이면 기존처럼 BaseDamage * DamageCoef 경로를 사용.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee", meta = (ClampMin = "-1.0"))
	float DamageOverride = -1.f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee")
	bool bCanHealMelee;
};

USTRUCT(BlueprintType)
struct FAscenduntSpread
{
	GENERATED_BODY()

	// 총구에서 발사되는 탄환이 퍼지는 각도. 0이면 완벽히 직선으로 날아감.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Recoil") float BaseSpreadDeg = 0.f;

	// 연속 발사 시 총구에서 발사되는 탄환이 퍼지는 각도가 얼마나 빨리 증가하는지. BaseSpreadDeg에 더해지는 값. 예: 0.5면 1발 발사 후 0.5도, 2발 후 1도, 3발 후 1.5도 퍼짐.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Recoil") float SpreadIncPerShot = 0.f;

	// 퍼짐이 얼마나 빨리 회복되는지. 초당 회복되는 퍼짐 각도. 예: 2면 1초마다 2도씩 퍼짐이 줄어듦.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Recoil") float SpreadRecoverPerSec = 2.0f;

	// 발사 시 조준선이 위로 들리는 정도. 예: 1이면 1발 발사 시 조준선이 1도 위로 들림.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Recoil") float RecoilPitch = 0.f;

	// 발사 시 조준선이 좌우로 흔들리는 정도. 양수면 오른쪽, 음수면 왼쪽으로 흔들림. 예: 0.5면 1발 발사 시 조준선이 최대 0.5도 오른쪽으로 흔들리고 -0.5면 최대 0.5도 왼쪽으로 흔들림.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Recoil") float RecoilYaw = 0.f;
};

USTRUCT(BlueprintType)
struct FAscenduntWeaponImpactVFX
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|VFX")
	TObjectPtr<UNiagaraSystem> WorldNiagara = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|VFX")
	TObjectPtr<UParticleSystem> WorldParticle = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|VFX")
	TObjectPtr<UNiagaraSystem> CharacterNiagara = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|VFX")
	TObjectPtr<UParticleSystem> CharacterParticle = nullptr;
};

USTRUCT(BlueprintType)
struct FAscenduntWeaponModeStats
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Mode") EAscenduntWeaponFireType FireType = EAscenduntWeaponFireType::Hitscan;

	// -------- Ammo ----------
	// 무한 탄창(탄 소모 없음, 탄 체크도 통과)
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Ammo")
	bool bInfiniteMag = false;

	// 탄창 크기
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Ammo", meta = (EditCondition = "!bInfiniteMag", EditConditionHides))
	int32 MagSize = 30;

	// 재장전 시간
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Ammo")
	float ReloadTime = 1.5f;

	// 한 발당 소모되는 탄약 수
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Ammo")
	int32 AmmoCostPerShot = 1;

	// -------- Fire ----------
	// shots/sec
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Fire")
	float RateOfFire = 20.f;

	// 트리거당 발사되는 총 발 수 (예: 3점사)
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Fire")
	int32 ShotsPerTrigger = 1;

	// -------- Damage / Heal ----------

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Damage")
	float BaseDamage = 15.f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Headshot")
	bool bAllowHeadshot = true;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Headshot", meta = (EditCondition = "bAllowHeadshot", EditConditionHides, ClampMin = "1.0"))
	float HeadshotDamageMultiplier = 2.0f;

	// 아군에게 발사하면 Heal할지 여부 (힐러만 체크)
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Heal")
	bool bCanHeal = false;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Heal", meta = (EditCondition = "bCanHeal", EditConditionHides, ClampMin = "0.0"))
	float BaseHeal = 50.f;

	// -------- Charge ----------
	/** X축: HeldTimeSeconds(초), Y축: DamageCoef (1.0 = 기본) */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Charge")
	TObjectPtr<UCurveFloat> ChargeDamageCoefCurve = nullptr;

	/** HeldTime clamp 상한 */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Charge", meta = (ClampMin = "0.0"))
	float MaxChargeTime = 3.0f;

	// -------- Hitscan falloff ----------
	// 사거리 증가에 따른 데미지 감소 곡선. X축은 거리, Y축은 데미지 스칼라(1 = 100%).
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Damage", meta = (EditCondition = "FireType == EAscenduntWeaponFireType::Hitscan", EditConditionHides))
	TObjectPtr<UCurveFloat> DistanceFalloffCurve = nullptr;

	// 최대 사거리 (DistanceFalloffCurve의 X축 범위 밖에서는 이 사거리까지 데미지가 0이 됨)
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Damage", meta = (EditCondition = "FireType == EAscenduntWeaponFireType::Hitscan", EditConditionHides))
	float MaxRange = 10000.f;

	// -------- Recoil ----------

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Recoil") FAscenduntSpread SpreadRecoil;

	// ====== Move penalty =======

	// 발사 중 이동 속도 스칼라. 1이면 이동 속도에 영향 없음, 0.5면 이동 속도가 절반으로 줄어듦.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Move") float MoveSpeedScalarWhileFiring = 1.f;

	// ADS 중 이동 속도 스칼라. 1이면 이동 속도에 영향 없음, 0.5면 이동 속도가 절반으로 줄어듦.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Move") float MoveSpeedScalarWhileADS = 0.5f;

	// ====== Projectile ======
	//
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Projectile", meta = (EditCondition = "FireType == EAscenduntWeaponFireType::Projectile", EditConditionHides))
	TSubclassOf<AAscenduntProjectile> ProjectileClass = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Projectile", meta = (EditCondition = "FireType == EAscenduntWeaponFireType::Projectile", EditConditionHides))
	float ProjectileSpeed = 2500.f;

	// 투사체 크기 스칼라(1=기본)
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Projectile", meta = (EditCondition = "FireType == EAscenduntWeaponFireType::Projectile", EditConditionHides))
	float ProjectileScale = 1.f;


	// 무효화 필드/태그에 의해 제거 가능한지(강력 투사체는 false)
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Projectile", meta = (EditCondition = "FireType == EAscenduntWeaponFireType::Projectile", EditConditionHides))
	bool bProjectileCanBeNullified = true;

	// 벽/월드(Blocking) 히트 시 즉시 Pool로 반환할지
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Projectile|Impact", meta = (EditCondition = "FireType == EAscenduntWeaponFireType::Projectile", EditConditionHides))
	bool bProjectileReturnToPoolOnWorldHit = false;

	// ====== Projectile Pooling ======
	/** 성능 비교를 위해 투사체 Object Pooling 사용 여부를 전환한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Projectile|Pooling", meta = (DisplayName = "Use Object Pooling", EditCondition = "FireType == EAscenduntWeaponFireType::Projectile", EditConditionHides))
	bool bUseProjectilePooling = true;

	/** 초기 Pool Size. 부족하면 새로 만들지 않고 Active 배열의 가장 오래된 투사체를 재사용한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Projectile|Pooling", meta = (ClampMin = "1", EditCondition = "FireType == EAscenduntWeaponFireType::Projectile && bUseProjectilePooling", EditConditionHides))
	int32 PoolSize = 16;

	/** 투사체가 Pool에 돌아가기까지의 시간 */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Projectile|Pooling", meta = (ClampMin = "0.0", EditCondition = "FireType == EAscenduntWeaponFireType::Projectile", EditConditionHides))
	float ProjectileActivateDuration = 1.f;

	// ====== Melee ======
	// 근접 몽타주 전체 재생 속도 배율. 공격 속도를 높이고 싶으면 이 값을 올린다.
	// Step별 MontagePlayRate와 곱해져 최종 재생 속도를 만든다.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee", meta = (ClampMin = "0.01"))
	float MeleePlayRate = 1.0f;

	// 콤보 확장용.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee")
	TArray<FAscenduntMeleeStep> MeleeSteps;

	// 모드 단위 근접 판정 디버그.
	// 개별 Step TraceProfile 디버그보다 우선 적용된다.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee|Debug")
	bool bDrawDebugMeleeHitBox = false;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee|Debug",
		meta = (
		EditCondition = "bDrawDebugMeleeHitBox",
		EditConditionHides,
		ClampMin = "0.0"
		))
	float MeleeDebugDrawDuration = 0.05f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee|Debug",
		meta = (
			EditCondition = "bDrawDebugMeleeHitBox",
			EditConditionHides,
			ClampMin = "0.1"
			))
	float MeleeDebugDrawThickness = 1.5f;


	// ====== View penalty ======

	// ADS 시 FOV.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|ADS") float ADS_FOV = 70.f;

	// ADS 시 감도 스칼라. 1이면 감도에 영향 없음, 0.5면 감도가 절반으로 줄어듦.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|ADS") float ADS_SensitivityScalar = 0.6f;


	// ======= VFX ======
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|VFX") TObjectPtr<UNiagaraSystem> FireVFXNiagara = nullptr;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|VFX") TObjectPtr<UParticleSystem> FireVFXParticle = nullptr;

	// Hitscan 탄도 Trail. User.User_TraceStart / User.User_TraceEnd Position 파라미터를 사용하는 Niagara System을 지정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|VFX",
		meta = (EditCondition = "FireType == EAscenduntWeaponFireType::Hitscan", EditConditionHides))
	TObjectPtr<UNiagaraSystem> FireTraceVFXNiagara = nullptr;

	// Normal and AOE impacts use the same shape so World/Character presentation stays paired.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|VFX")
	FAscenduntWeaponImpactVFX ImpactVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|VFX|AOE")
	FAscenduntWeaponImpactVFX AOEImpactVFX;

	// Legacy per-field values are kept only so existing Weapon DAs migrate on PostLoad.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|VFX", meta = (DeprecatedProperty, DeprecationMessage = "Use ImpactVFX.WorldNiagara"))
	TObjectPtr<UNiagaraSystem> FireImpactWorldVFXNiagara = nullptr;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|VFX", meta = (DeprecatedProperty, DeprecationMessage = "Use ImpactVFX.WorldParticle"))
	TObjectPtr<UParticleSystem> FireImpactWorldVFXParticle = nullptr;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|VFX", meta = (DeprecatedProperty, DeprecationMessage = "Use ImpactVFX.CharacterNiagara"))
	TObjectPtr<UNiagaraSystem> WeaponImpactCharacterVFXNiagara = nullptr;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|VFX", meta = (DeprecatedProperty, DeprecationMessage = "Use ImpactVFX.CharacterParticle"))
	TObjectPtr<UParticleSystem> WeaponImpactCharacterVFXParticle = nullptr;
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use ImpactVFX.CharacterNiagara"))
	TObjectPtr<UNiagaraSystem> FireImpactCharacterVFXNiagara_DEPRECATED = nullptr;
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use ImpactVFX.CharacterParticle"))
	TObjectPtr<UParticleSystem> FireImpactCharacterVFXParticle_DEPRECATED = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|VFX") TObjectPtr<USoundCue> FireSoundSFX = nullptr;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|VFX") TObjectPtr<USoundCue> FireImpactWorldSoundSFX = nullptr;

	// ======== Animation =========
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Animation") TObjectPtr<UAnimMontage> WeaponFireMontage = nullptr;
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Animation") TObjectPtr<UAnimMontage> WeaponReloadMontage = nullptr;
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Animation") TObjectPtr<UAnimMontage> HeroFireMontage = nullptr;
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Animation") TObjectPtr<UAnimMontage> HeroReloadMontage = nullptr;

	// 1인칭 전용 몽타주. Mesh1P 에서만 재생되며 복제하지 않는다(로컬 오너만 보므로).
	// 비워두면 1인칭에서 팔이 움직이지 않을 뿐, 3P 재생과 게임플레이 타이밍에는 영향이 없다.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Animation") TObjectPtr<UAnimMontage> HeroFireMontage1P = nullptr;
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Animation") TObjectPtr<UAnimMontage> HeroReloadMontage1P = nullptr;
};

USTRUCT(BlueprintType)
struct FAscenduntWeaponModeConfig
{
	GENERATED_BODY()

	// Mode.Primary / Mode.Sniper 등
	UPROPERTY(EditDefaultsOnly, Category = "Mode") FGameplayTag ModeTag;
	UPROPERTY(EditDefaultsOnly, Category = "Mode") FAscenduntWeaponModeStats Stats;
};

/**
 *
 */
UCLASS(BlueprintType)
class ASCENDUNT_API UWeaponDefinitionData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UWeaponDefinitionData(const FObjectInitializer& ObjectInitializer);
	virtual void PostLoad() override;

	// 무기 식별 태그(옵션)
	UPROPERTY(EditDefaultsOnly, Category = "Weapon") FGameplayTag WeaponTag;

	// 장착 시 ASC에 부여할 능력 세트(발사/재장전/모드전환/차징 등)
	UPROPERTY(EditDefaultsOnly, Category = "Weapon") TObjectPtr<UAscenduntAbilitySet> WeaponAbilitySet;

	/**
	 * 무기 스탯을 Attribute로 밀어넣을 공용 GE(무한 지속 권장).
	 * 이 GE는 모든 Modifier가 SetByCaller로 되어 있어야 함.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon") TSubclassOf<UGameplayEffect> WeaponStatsEffect;

	// 모드 배열(라이플/저격 같은 경우 2개)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon") TArray<FAscenduntWeaponModeConfig> Modes;


};
