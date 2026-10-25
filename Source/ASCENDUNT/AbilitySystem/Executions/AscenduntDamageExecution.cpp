// Fill out your copyright notice in the Description page of Project Settings.


#include "AscenduntDamageExecution.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntHealthSet.h"
#include "ASCENDUNT/AbilitySystem/AscenduntGameplayEffectContext.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySourceInterface.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntCharacterStatSet.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/Character/HitCollision/AscHitCollisionComponent.h"
#include "AbilitySystemGlobals.h"

#include "Engine/World.h"
// #include "GameplayTagsManager.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"



UAscenduntDamageExecution::UAscenduntDamageExecution()
{
}

void UAscenduntDamageExecution::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
#if WITH_SERVER_CODE
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	FAscenduntGameplayEffectContext* TypedContext = FAscenduntGameplayEffectContext::ExtractEffectContext(Spec.GetContext());
	check(TypedContext);

	const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();
	UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();

	FAggregatorEvaluateParameters EvalParams;
	EvalParams.SourceTags = SourceTags;
	EvalParams.TargetTags = TargetTags;

	// RespawnRoomVolume grants this loose tag on the server. Damage execution is
	// the final authoritative guard, so spawn-room invulnerability does not rely
	// on client UI or collision settings.
	if (TargetTags && TargetTags->HasTag(AscenduntGameplayTags::Status_Invincible))
	{
		return;
	}

	// (추가) 반사/특수 데미지용: SetByCaller(Data.DamageOverride) 우선
	const float OverrideDamage = Spec.GetSetByCallerMagnitude(AscenduntGameplayTags::Data_DamageOverride, false, -1.f);
	const bool bHasOverrideDamage = (OverrideDamage >= 0.f);

	float DamageDone = 0.f;

	if (bHasOverrideDamage)
	{
		DamageDone = FMath::Max(OverrideDamage, 0.f);
	}
	else
	{
		// 기존 Lyra 방식: BaseDamage 캡처 + 감쇠
		float BaseDamage = Spec.GetSetByCallerMagnitude(
			AscenduntGameplayTags::Data_Weapon_BaseDamage,
			false,
			0.f
		);
		BaseDamage = FMath::Max(BaseDamage, 0.f);

		const float DamageCoef = Spec.GetSetByCallerMagnitude(AscenduntGameplayTags::Data_DamageCoef, false, 1.f);
		const float DamageFlat = Spec.GetSetByCallerMagnitude(AscenduntGameplayTags::Data_DamageFlat, false, 0.f);
		BaseDamage = (BaseDamage * FMath::Max(0.f, DamageCoef)) + DamageFlat;

		const AActor* EffectCauser = TypedContext->GetEffectCauser();
		const FHitResult* HitResult = TypedContext->GetHitResult();

		AActor* HitActor = nullptr;
		FVector ImpactLocation = FVector::ZeroVector;

		if (HitResult)
		{
			HitActor = HitResult->HitObjectHandle.FetchActor();
			ImpactLocation = HitResult->ImpactPoint;
		}

		if (!HitActor && TargetASC)
		{
			HitActor = TargetASC->GetAvatarActor_Direct();
			if (HitActor)
			{
				ImpactLocation = HitActor->GetActorLocation();
			}
		}

		// =========== 헤드 샷 ==============
		bool bIsHeadshot = false;

		UAbilitySystemComponent* CauserASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(EffectCauser);
		if (!CauserASC) return;

		if (HitResult)
		{
			if (const UAscHitCollisionComponent* HitCollision = Cast<UAscHitCollisionComponent>(HitResult->GetComponent()))
			{
				bIsHeadshot = HitCollision->IsHeadZone() &&
					CauserASC->HasMatchingGameplayTag(AscenduntGameplayTags::Data_Weapon_AllowHeadshot);
			}
		}


		// ========= 거리 감쇠 ==============
		double Distance = WORLD_MAX;
		if (TypedContext->HasOrigin())
		{
			Distance = FVector::Dist(TypedContext->GetOrigin(), ImpactLocation);
		}
		else if (EffectCauser)
		{
			Distance = FVector::Dist(EffectCauser->GetActorLocation(), ImpactLocation);
		}

		// ========== 거리 감쇠, 헤드샷 배율 계산 ============

		float DistanceAttenuation = 1.0f;

		float HeadshotMultiplier = 1.0f;

		if (const IAscenduntAbilitySourceInterface* AbilitySource = TypedContext->GetAbilitySource())
		{
			DistanceAttenuation = AbilitySource->GetDistanceAttenuation(Distance, SourceTags, TargetTags);

			if (bIsHeadshot)
			{
				HeadshotMultiplier = AbilitySource->GetHeadshotDamageMultiplier(SourceTags, TargetTags);
			}
		}

		if (HitResult)
		{
			const UAscHitCollisionComponent* HitCollision = Cast<UAscHitCollisionComponent>(HitResult->GetComponent());
			UE_LOG(LogTemp, Display, TEXT("Hit Zone: %s"),
				HitCollision ? *UEnum::GetValueAsString(HitCollision->GetHitZone()) : TEXT("None"));
		}
		UE_LOG(LogTemp, Display, TEXT("Distance: %f, DistanceAttenuation: %f"), float(Distance), DistanceAttenuation);
		UE_LOG(LogTemp, Display, TEXT("DamageCoef: %f"), DamageCoef);

		// ============ 최종 데미지 증가 ================
		DamageDone = BaseDamage
			* FMath::Max(0.f, DistanceAttenuation)
			* FMath::Max(1.f, HeadshotMultiplier);
		if (DamageDone > 0.f)
		{
			DamageDone = FMath::Max(DamageDone, 1.f);
		}
	}

	// Common outgoing-damage scalar. Applied after every weapon/skill/override branch so
	// Temporary buffs/debuffs affect the complete damage pipeline, including override damage.
	if (DamageDone > 0.0f)
	{
		float DamageDealtScalar = 1.0f;
		if (UAbilitySystemComponent* SourceASC = ExecutionParams.GetSourceAbilitySystemComponent())
		{
			if (const UAscenduntCharacterStatSet* SourceStatSet = SourceASC->GetSet<UAscenduntCharacterStatSet>())
			{
				DamageDealtScalar = SourceStatSet->GetDamageDealtScalar();
			}
		}

		DamageDone *= FMath::Max(0.0f, DamageDealtScalar);

		float DamageTakenScalar = 1.0f;
		if (TargetASC)
		{
			if (const UAscenduntCharacterStatSet* TargetStatSet = TargetASC->GetSet<UAscenduntCharacterStatSet>())
			{
				DamageTakenScalar = TargetStatSet->GetDamageTakenScalar();
			}
		}
		DamageDone *= FMath::Max(0.0f, DamageTakenScalar);
	}

	// 모든 배율/감쇠 계산이 끝난 최종 데미지를 정수 단위로 반올림한다.
	// Attribute 타입은 float이므로 값만 정수 형태(예: 12.0f)로 유지한다.
	if (DamageDone > 0.f)
	{
		DamageDone = FMath::Max(1.f, FMath::RoundToFloat(DamageDone));

		// Lyra pattern: write to HealthSet.Damage(meta); HealthSet consumes the real pools.
		OutExecutionOutput.AddOutputModifier(
			FGameplayModifierEvaluatedData(UAscenduntHealthSet::GetDamageAttribute(), EGameplayModOp::Additive, DamageDone)
		);
	}
#endif
}
